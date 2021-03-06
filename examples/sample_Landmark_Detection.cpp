/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2008-2013, Itseez Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of Itseez Inc. may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the copyright holders or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/
#include "opencv2/core.hpp"
#include "opencv2/objdetect.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "../include/train_shape.hpp"
#include "opencv2/videoio.hpp"
#include <vector>
#include <iostream>

using namespace std;
using namespace cv;

static void help()
{
    cout << "\nThis program demonstrates the Detection of Facial Landmarks by Vahid Kazemi.\n"
            "The module is capable of running on single/multiple images as well as on video and live input.\n"
            "This approach works with LBP, HOG and HAAR based face detectors.\n"
            "Please use the same face detector as used while training for most accurate results.\n"
            "Usage:\n"
            "./Landmark_detection_ex [-cascade=<face detector model>(optional)this is the cascade module used for face detection(deafult: Haar frontal face detector 2)]\n"
               "   [-model=<path to trained model> specifies the path to model trained using training module]\n"
               "   [@filename(For image: provide path to image, For multiple images: Provide a txt file with path to images, For video input: Provide path to video, For live input: Leave blank)]\n\n"
            "for one call:\n"
            "./landmark_detection_ex -cascade=\"../data/haarcascade_frontalface_alt2.xml\" -model=\"../data/68_landmarks_face_align.dat\" image1.png\n"
            "Using OpenCV version " << CV_VERSION << "\n" << endl;
}

int main(int argc, const char** argv)
{
    VideoCapture capture;
    string cascadeName, inputName;
    CascadeClassifier cascade;
    string poseTree;
    Mat image, frame;
    cv::CommandLineParser parser(argc ,argv,
            "{help h||}"
            "{cascade | ../data/haarcascade_frontalface_alt2.xml|}"  //Only HAAR based detectors
            "{model| ../data/68_landmarks_face_align.dat |}"        //will work as the model is
            "{@filename||}"                                         //trained using HAAR.
        );
    if(parser.has("help"))
    {
        help();
        return 0;
    }
    cascadeName = parser.get<string>("cascade");
    if( !cascade.load( cascadeName ) )
    {
        cerr << "ERROR: Could not load classifier cascade" << endl;
        help();
        return -1;
    }
    poseTree = parser.get<string>("model");
    KazemiFaceAlignImpl predict;
    ifstream fs(poseTree, ios::binary);
    if(!fs.is_open())
    {
        cerr << "Failed to open trained model file " << poseTree << endl;
        help();
        return 1;
    }
    vector< vector<regressionTree> > forests;
    vector< vector<Point2f> > pixelCoordinates;
    predict.loadTrainedModel(fs, forests, pixelCoordinates);
    predict.calcMeanShapeBounds();
    vector< vector<Point2f>> result;
    inputName = parser.get<string>("@filename");
    if (!parser.check())
    {
        parser.printErrors();
        return 0;
    }
    if( inputName.empty() || (isdigit(inputName[0]) && inputName.size() == 1) )
    {
        int camera = inputName.empty() ? 0 : inputName[0] - '0';
        if(!capture.open(camera))
            cout << "Capture from camera #" <<  camera << " didn't work" << endl;
    }
    else if( inputName.size() )
    {
        image = imread( inputName, 1 );
        if( image.empty() )
        {
            if(!capture.open( inputName ))
                cout << "Could not read " << inputName << endl;
        }
    }
    else
    {
        image = imread( "../data/lena.jpg", 1 );
        if(image.empty()) cout << "Couldn't read ../data/lena.jpg" << endl;
    }
    if( capture.isOpened() )
    {
        cout << "Video capturing has been started ..." << endl;
        for(;;)
        {
            capture >> frame;
            if( frame.empty() )
                break;
            resize(frame, frame, Size(460,460));
            vector<Rect> faces  = predict.faceDetector(frame, cascade);
            result = predict.getFacialLandmarks(frame, faces, forests, pixelCoordinates);
            predict.renderDetectionsperframe(frame, faces, result, Scalar(0,255,0), 2);
            char c = (char)waitKey(10);
            if( c == 27 || c == 'q' || c == 'Q' )
                break;
        }
    }
    else
    {
        cout << "Detecting landmarks in " << inputName << endl;
        if( !image.empty() )
        {
            resize(image, image, Size(460,460));
            vector<Rect> faces = predict.faceDetector(image, cascade);
            result = predict.getFacialLandmarks(image, faces, forests, pixelCoordinates);
            predict.renderDetectionsperframe(image, faces, result, Scalar(0, 255, 0), 2);
            waitKey(0);
        }
        else if( !inputName.empty() )
        {
            /* assume it is a text file containing the
            list of the image filenames to be processed - one per line */
            FILE* f = fopen( inputName.c_str(), "rt" );
            if( f )
            {
                char buf[1000+1];
                while( fgets( buf, 1000, f ) )
                {
                    int len = (int)strlen(buf);
                    while( len > 0 && isspace(buf[len-1]) )
                        len--;
                    buf[len] = '\0';
                    cout << "file " << buf << endl;
                    image = imread( buf, 1 );
                    if( !image.empty() )
                    {
                        resize(image, image, Size(460,460));
                        vector<Rect> faces = predict.faceDetector(image, cascade);
                        result = predict.getFacialLandmarks(image, faces, forests, pixelCoordinates);
                        predict.renderDetectionsperframe(image, faces, result, Scalar(0,255,0), 2);
                        char c = (char)waitKey(0);
                        if( c == 27 || c == 'q' || c == 'Q' )
                            break;
                    }
                    else
                    {
                        cerr << "Aw snap, couldn't read image " << buf << endl;
                    }
                }
                fclose(f);
            }
        }
    }
    return 0;
}