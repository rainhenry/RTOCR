#!/usr/bin/python3
#*************************************************************
#
#   程序名称:Python模块子程序
#   程序版本:REV 0.1
#   创建日期:20231224
#   设计编写:rainhenry
#   作者邮箱:rainhenry@savelife-tech.com
#   版    权:SAVELIFE Technology of Yunnan Co., LTD
#   开源协议:GPL
#
#   版本修订
#       REV 0.1   20231224      rainhenry    创建文档
#
#*************************************************************
import paddlehub as hub
import numpy

def ocr_init_fast():
    ocr = hub.Module(name="chinese_ocr_db_crnn_mobile", enable_mkldnn=True)
    return ocr

def ocr_init_precise():
    ocr = hub.Module(name="chinese_ocr_db_crnn_server", enable_mkldnn=True)
    return ocr

def ocr_ex(img_raw: numpy.ndarray, ocr):
    result = ocr.recognize_text(images=[img_raw])
    return result[0]['data']

