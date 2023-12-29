#!/bin/bash
hub run chinese_ocr_db_crnn_server --input_path "RTOCR/test1.bmp"
hub run chinese_ocr_db_crnn_mobile --input_path "RTOCR/test1.bmp"
