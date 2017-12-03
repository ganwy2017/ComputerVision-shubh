#!/usr/bin/env python
import rospy
import cv2
import numpy as np
import cv_bridge
from sensor_msgs.msg import Image

def nothing(x):
    pass

def topview(image,t1):
    if len(image.shape)==2:
        height,width = image.shape
        src = np.array([[0,height/2],[width,height/2],[width,height],[0,height]],dtype='float32')
        dest = np.array([[0,0],[width,0],[width-t1,height],[t1,height]],dtype='float32')
        h, status = cv2.findHomography(src, dest)
        imgd = np.zeros((height,width),dtype='uint8')
        imgd = cv2.warpPerspective(image, h, (width,height))
    else:
        height,width,col = image.shape
        src = np.array([[0,height/2],[width,height/2],[width,height],[0,height]],dtype='float32')
        dest = np.array([[0,0],[width,0],[width-t1,height],[t1,height]],dtype='float32')
        h, status = cv2.findHomography(src, dest)
        imgd = np.zeros((height,width,col),dtype='uint8')
        imgd[:,:,0] = cv2.warpPerspective(image[:,:,0], h, (width,height))
        imgd[:,:,1] = cv2.warpPerspective(image[:,:,1], h, (width,height))
        imgd[:,:,2] = cv2.warpPerspective(image[:,:,2], h, (width,height))
    return imgd,h

rospy.init_node('top_view_maker')
vidFile = cv2.VideoCapture(0)
ret, frame = vidFile.read()
cv2.namedWindow('image')
height,width,_ = frame.shape
cv2.createTrackbar('t1','image',0,width,nothing)
while ret:
    ret, frame = vidFile.read()
    t1 = cv2.getTrackbarPos('t1','image')
    frame,h = topview(frame,t1)
    cv2.imshow('image',frame)
    key = cv2.waitKey(30)
    if key==27 or key==1048603:
        print h
        file1 = open("top_view.txt",'w')
        file1.write(np.array_str(h))
        break
exit()