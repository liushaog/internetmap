package com.peer1.internetmap;

import java.io.UnsupportedEncodingException;

import org.apache.http.entity.StringEntity;
import org.json.JSONException;
import org.json.JSONObject;

import com.loopj.android.http.AsyncHttpClient;
import com.loopj.android.http.AsyncHttpResponseHandler;

import android.util.Log;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.PopupWindow;
import android.widget.ProgressBar;
import android.widget.Toast;

public class SalesPopup extends PopupWindow{
    private static String TAG = "SalesPopup";
    private InternetMap mContext;

    public SalesPopup(final InternetMap context, View view) {
        super(view, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        setOutsideTouchable(true);
        setFocusable(true);
        mContext = context;

        View closeButton = getContentView().findViewById(R.id.closeBtn);
        closeButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View arg0) {
                SalesPopup.this.dismiss();
            }
        });

        View submitButton = getContentView().findViewById(R.id.submitButton);
        submitButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View arg0) {
                //validate (split so that we do not short-circuit past any check).
                boolean valid = validateName();
                valid = validateEmail() && valid;
                
                if (valid) {
                    submit();
                }
            }
        });
    }
    
    private boolean validateName() {
        EditText nameEdit = (EditText) getContentView().findViewById(R.id.nameEdit);
        if (nameEdit.getText().length() == 0) {
            //ideally we'd use setError, but it's unbelievably buggy, so we're stuck with a toast :P
            Toast.makeText(mContext, mContext.getString(R.string.requiredName),  Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }

    private boolean validateEmail() {
        EditText edit = (EditText) getContentView().findViewById(R.id.emailEdit);
        if (edit.getText().length() == 0) {
            //ideally we'd use setError, but it's unbelievably buggy, so we're stuck with a toast :P
            Toast.makeText(mContext, mContext.getString(R.string.requiredEmail),  Toast.LENGTH_SHORT).show();
            return false;
        }
        return true;
    }
    
    private void submit() {
        if (!mContext.haveConnectivity()) {
            return;
        }
        Log.d(TAG, "submit");
        
        //package up the data
        JSONObject postData = new JSONObject();
        try {
            postData.put("company", "Unknown");
            postData.put("LeadSource", "Map of the Internet");
            postData.put("Website_Source__c", "Android"); //FIXME ?
            
            EditText nameEdit = (EditText) getContentView().findViewById(R.id.nameEdit);
            postData.put("fullName", nameEdit.getText().toString());
            EditText emailEdit = (EditText) getContentView().findViewById(R.id.emailEdit);
            postData.put("email", emailEdit.getText().toString());
            EditText phoneEdit = (EditText) getContentView().findViewById(R.id.phoneEdit);
            postData.put("phone", phoneEdit.getText().toString());
        } catch (JSONException e) {
            //I'm not feeding it anything risky, it'll be fine.
            e.printStackTrace();
            return;
        }
        
        StringEntity entity = null;
        try{
            entity = new StringEntity(postData.toString());
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            return;
            //FIXME Do we want to notify the user if we have some kind of encoding exception?
        }
        entity.setContentType("application/json");
        
        Log.d(TAG, "starting httpclient");
        
        //prevent doubleclicking
        final Button button = (Button) getContentView().findViewById(R.id.submitButton);
        button.setEnabled(false);
        //start spinning
        final ProgressBar progress = (ProgressBar) getContentView().findViewById(R.id.progressBar);
        progress.setVisibility(View.VISIBLE);
        
        //send it off to the internet
        AsyncHttpClient client = new AsyncHttpClient();
        client.post(null, "http://www.peer1.com/lead-submit", entity, "application/json", new AsyncHttpResponseHandler(){
            @Override
            public void onStart(){
                Log.d(TAG, "started");
            }
            @Override
            public void onSuccess(String response) {
                Log.d(TAG, String.format("Success! response: %s", response));
                Toast.makeText(mContext, mContext.getString(R.string.submitSuccess),  Toast.LENGTH_SHORT).show();
                SalesPopup.this.dismiss();
            }
            @Override
            public void onFailure(Throwable error, String content) {
                Log.d(TAG, String.format("error: '%s' content: '%s'", error.getMessage(), content));
                int messageId;
                if (error.getMessage().equals("Unprocessable Entity")) {
                    messageId = R.string.submitFailInvalid;
                } else {
                    messageId = R.string.submitFail;
                }
                Toast.makeText(mContext, mContext.getString(messageId),  Toast.LENGTH_SHORT).show();
                button.setEnabled(true);
                progress.setVisibility(View.INVISIBLE);
            }
            @Override
            public void onFinish() {
                Log.d(TAG, "ended");
            }
        });
        
    }

}