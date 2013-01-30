package com.peer1.internetmap;

import java.io.InputStream;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.graphics.Point;
import android.os.Build;
import android.os.Handler;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.os.Bundle;
import android.widget.AdapterView;
import android.widget.ListView;
import android.widget.PopupWindow;
import android.widget.Toast;
import android.view.ScaleGestureDetector;
import android.view.ViewGroup.LayoutParams;
import android.view.GestureDetector;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceView;
import android.view.SurfaceHolder;
import android.view.View;
import android.support.v4.view.GestureDetectorCompat;
import android.util.Log;
import com.loopj.android.http.AsyncHttpResponseHandler;

public class InternetMap extends Activity implements SurfaceHolder.Callback {

    private static String TAG = "InternetMap";
    private GestureDetectorCompat mGestureDetector;
    private ScaleGestureDetector mScaleDetector;
    private RotateGestureDetector mRotateDetector;
    
    private MapControllerWrapper mController;

    private VisualizationPopupWindow visualizationPopup;
    private NodePopup nodePopup;
    private Handler mHandler; //handles threadsafe messages

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        requestWindowFeature(android.view.Window.FEATURE_NO_TITLE);

        Log.i(TAG, "onCreate()");

        nativeOnCreate();

        setContentView(R.layout.main);
        SurfaceView surfaceView = (SurfaceView) findViewById(R.id.surfaceview);
        surfaceView.getHolder().addCallback(this);

        mGestureDetector = new GestureDetectorCompat(this, new MyGestureListener());
        mScaleDetector = new ScaleGestureDetector(this, new ScaleListener());
        mRotateDetector = new RotateGestureDetector(this, new RotateListener());
        
        mController = new MapControllerWrapper();
        mHandler = new Handler();
    }

    public String readFileAsString(String filePath) throws java.io.IOException {
        Log.i(TAG, String.format("Reading %s", filePath));
        InputStream input = getAssets().open(filePath);

        int size = input.available();
        byte[] buffer = new byte[size];
        input.read(buffer);
        input.close();

        // byte buffer into a string
        return new String(buffer);
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume()");
        nativeOnResume();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.i(TAG, "onPause()");
        nativeOnPause();
    }

    @Override
    protected void onStop() {
        super.onDestroy();
        Log.i(TAG, "onStop()");
        nativeOnStop();
    }

    @SuppressWarnings("deprecation")
    @SuppressLint("NewApi")
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        android.view.Display display = getWindowManager().getDefaultDisplay();
        
        int width, height;
        //getSize is only available from api 13
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR2) {
            Point size = new Point();
            display.getSize(size);
            width = size.x;
            height = size.y;
        } else {
            width = display.getWidth();
            height = display.getHeight();
        }

        Log.i(TAG, String.format("screen %d %d ", width, height, getResources().getDisplayMetrics().density));
        Log.i(TAG, String.format("surface %d %d %.2f", w, h, getResources().getDisplayMetrics().density));
        nativeSetSurface(holder.getSurface(), getResources().getDisplayMetrics().density);
    }

    public void surfaceCreated(SurfaceHolder holder) {
    }

    public void surfaceDestroyed(SurfaceHolder holder) {
        nativeSetSurface(null, 1.0f);
    }

    //UI stuff

    @Override
    public boolean onTouchEvent(MotionEvent event){
        mScaleDetector.onTouchEvent(event);
        mRotateDetector.onTouchEvent(event);
        mGestureDetector.onTouchEvent(event);
        return super.onTouchEvent(event);
    }

    public void visualizationsButtonPressed(View view) {
        if (visualizationPopup == null) {
            LayoutInflater layoutInflater = (LayoutInflater)getBaseContext().getSystemService(LAYOUT_INFLATER_SERVICE);
            View popupView = layoutInflater.inflate(R.layout.visualizationview, null);
            visualizationPopup = new VisualizationPopupWindow(this, popupView);
            visualizationPopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
                public void onDismiss() {
                    visualizationPopup = null;
                }
            });
            visualizationPopup.showAsDropDown(findViewById(R.id.visualizationsButton));
        }
    }

    public void youAreHereButtonPressed(View view) {
        //TODO: stop timeline if active
        //TODO: animate button while fetching
        //do an ASN request to get the user's ASN
        ASNRequest.fetchCurrentASNWithResponseHandler(new AsyncHttpResponseHandler() {
            @Override
            public void onStart() {
                // Initiated the request
                Log.d(TAG, "asnrequest start");
            }
            @Override
            public void onFinish() {
                // Initiated the request
                Log.d(TAG, "asnrequest finish");
            }

            @Override
            public void onSuccess(String response) {
                //expected response format: {"payload":"ASxxxx"}
                //quick&dirty parsing FIXME ASNRequest should handle this, and do proper JSON deserialization
                try {
                    String asnString = response.substring(14, response.length()-2);
                    Log.d(TAG, String.format("asn: %s", asnString));
                    //yay, an ASN! hand it back to InternetMap
                    selectNodeForASN(asnString);
                } catch (IndexOutOfBoundsException e) {
                    //TODO toast failure message
                    Log.d(TAG, String.format("can't parse response: %s", response));
                    showError(getString(R.string.asnBadResponse));
                } finally {}
            }
        
            @Override
            public void onFailure(Throwable e, String response) {
                //tell the user
                //FIXME: outputting the raw error response is bad. how can we make it userfriendly?
                String message = String.format(getString(R.string.asnfail), response);
                showError(message);
                Log.d(TAG, message);
            }
        });
    }
    
    public void showError(String message) {
        //TODO: I'm not sure if a dialog or a toast is most appropriate for errors.
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }
    
    public void selectNodeForASN(String asn) {
        NodeWrapper node = mController.nativeNodeByAsn(asn);
        if (node != null) {
            Log.d(TAG, "TODO selectNodeForASN");
            selectNode(node);
            //TODO: target that node
        } else {
            showError(String.format(getString(R.string.asnNullNode), asn));
        }
    }
    
    public void selectNode(NodeWrapper node) {
        //FIXME: what shows the node popup? do we need to do nodeSearchDelegateDone?
        mController.nativeUpdateTargetForIndex(node.index);
    }

    //called from c++
    public void showNodePopup() {
        Log.d(TAG, "showNodePopup");
        //get the current node
        int index = mController.nativeTargetNodeIndex();
        Log.d(TAG, String.format("node at index %d", index));
        NodeWrapper node = mController.nativeNodeAtIndex(index);
        if (node == null) {
            Log.d(TAG, "is null");
            if (nodePopup != null) {
                nodePopup.dismiss();
            }
        } else {
            //node is ok; show the popup
            Log.d(TAG, String.format("has index %d and asn %s", node.index, node.asn));
            if (nodePopup == null) {
                LayoutInflater layoutInflater = (LayoutInflater)getBaseContext().getSystemService(LAYOUT_INFLATER_SERVICE);
                View popupView = layoutInflater.inflate(R.layout.nodeview, null);
                nodePopup = new NodePopup(this, popupView);
                nodePopup.setOnDismissListener(new PopupWindow.OnDismissListener() {
                    public void onDismiss() {
                        nodePopup = null;
                    }
                });
            }
            nodePopup.setNode(node);
            nodePopup.showAsDropDown(findViewById(R.id.visualizationsButton)); //FIXME show by node
        }
    }
    
    //callbacks from the nodePopup UI
    public void dismissNodePopup(View unused) {
        nodePopup.dismiss();
    }
    
    public void runTraceroute(View unused){
        Log.d(TAG, "TODO: traceroute");
    }

    //native wrappers
    public native void nativeOnCreate();
    public native void nativeOnResume();
    public native void nativeOnPause();
    public native void nativeOnStop();
    public native void nativeSetSurface(Surface surface, float density);
    
    //threadsafe callbacks for c++
    public void threadsafeShowNodePopup() {
        mHandler.post(new Runnable() {
            public void run() {
                showNodePopup();
            }
        });
    }
    

    static {
        System.loadLibrary("internetmaprenderer");
    }

    //simple one-finger gestures (eg. pan)
    class MyGestureListener extends GestureDetector.SimpleOnGestureListener {
        private float distance2radians(float distance) {
            return -0.01f * distance;
        }
        private float velocityAdjust(float velocity) {
            return 0.002f * velocity;
        }

        @Override
        public boolean onDown(MotionEvent event) { 
            Log.d(TAG, "onDown");
            mController.nativeHandleTouchDownAtPoint(event.getX(), event.getY());
            return true;
        }

        @Override
        public boolean onScroll(MotionEvent e1, MotionEvent e2, float distanceX,
                float distanceY) {
            Log.d(TAG, String.format("onScroll: x %f y %f", distanceX, distanceY));
            mController.nativeRotateRadiansXY(distance2radians(distanceX), distance2radians(distanceY));
            return true;
        }
        
        @Override
        public boolean onFling(MotionEvent event1, MotionEvent event2, 
                float velocityX, float velocityY) {
            Log.d(TAG, String.format("onFling: vx %f vy %f", velocityX, velocityY));
            mController.nativeStartMomentumPanWithVelocity(velocityAdjust(velocityX), velocityAdjust(velocityY));
            return true;
        }

        @Override
        //note: if double tap is used this should probably s/Up/Confirmed
        public boolean onSingleTapUp(MotionEvent e) {
            Log.d(TAG, "tap!");
            mController.nativeSelectHoveredNode();
            //TODO: iOS does some deselect stuff if that call failed.
            //but, I think maybe that should just happen inside the controller automatically.
            return true;
        }
    }

    //zoom gesture
    private class ScaleListener extends ScaleGestureDetector.SimpleOnScaleGestureListener {
        @Override
        public boolean onScale(ScaleGestureDetector detector) {
            float scale = detector.getScaleFactor() - 1;
            Log.d(TAG, String.format("scale: %f", scale));
            mController.nativeZoomByScale(scale);
            return true;
        }

        @Override
        public void onScaleEnd(ScaleGestureDetector detector) {
            float scale = detector.getScaleFactor() - 1;
            Log.d(TAG, String.format("scaleEnd: %f", scale));
            mController.nativeStartMomentumZoomWithVelocity(scale * 50);
        }
    }

    //2-finger rotate gesture
    private class RotateListener extends RotateGestureDetector.SimpleOnRotateGestureListener {
        @Override
        public boolean onRotate(RotateGestureDetector detector) {
            float rotate = detector.getRotateFactor();
            Log.d(TAG, String.format("!!rotate: %f", rotate));
            mController.nativeRotateRadiansZ(-rotate);
            return true;
        }

        @Override
        public void onRotateEnd(RotateGestureDetector detector) {
            float velocity = detector.getRotateFactor(); //FIXME not actually velocity. always seems to be 0
            Log.d(TAG, String.format("!!!!rotateEnd: %f", velocity));
            mController.nativeStartMomentumRotationWithVelocity(velocity * 50);
        }
    }
    
}
