//
//  RootVC.swift
//  Internet Map
//
//  Created by Nigel Brooke on 2017-11-16.
//  Copyright © 2017 Peer1. All rights reserved.
//

import UIKit
import ARKit

private class CameraDelegate: NSObject, ARSessionDelegate {
    let renderer: ViewController

    init(renderer: ViewController) {
        self.renderer = renderer
        super.init()
    }

    public func session(_ session: ARSession, didUpdate frame: ARFrame) {
        renderer.overrideCamera(frame.camera.viewMatrix(for: .portrait), projection: frame.camera.projectionMatrix(for: .portrait, viewportSize: CGSize(width:1080, height: 1920), zNear: 0.05, zFar: 100))
    }
}

public class RootVC: UIViewController {
    private var rendererVC: ViewController!
    private var arkitView: ARSCNView!
    private var cameraDelegate : CameraDelegate!

   public override func viewDidLoad() {
        super.viewDidLoad()

        arkitView = ARSCNView()
        arkitView.frame = view.frame
        view.addSubview(arkitView)

        if UIDevice.current.userInterfaceIdiom == .phone {
            rendererVC = ViewController(nibName: "ViewController_iPhone", bundle: nil)
        }
        else {
            rendererVC = ViewController(nibName: "ViewController_iPad", bundle: nil)
        }

        rendererVC.view.frame = view.frame
        view.addSubview(rendererVC.view)
        addChildViewController(rendererVC)


        cameraDelegate = CameraDelegate(renderer: rendererVC)
        arkitView.session.delegate = cameraDelegate

        let configuration = ARWorldTrackingConfiguration()
        configuration.planeDetection = .horizontal
        arkitView.session.run(configuration)
    }
}
