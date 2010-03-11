#!/usr/bin/clisp -q -norc
;;; -*- Mode: Lisp -*-

(defpackage :js.tools.rw-log
  (:use :common-lisp :regexp))
(in-package :js.tools.rw-log)

;;; Usage: rw-log.lisp 0xF7CE0000 0x4000 log.txt

(defun hexparse (s)
  (values
   (parse-integer
    s :radix 16
    :start (if (string= "0x" s :end2 (min (length s) 2))
	       2 0))))

(let ((offset (hexparse (first ext:*args*)))
      (size (hexparse (second ext:*args*))))
  (with-open-file (in (third ext:*args*))
    (loop 
       for line = (read-line in nil nil)
       while line
       do (multiple-value-bind (match? sRW sAddr sLen sVal)
	      (match ".*PCI(WRITE|READ) ([0-9a-f]+) \\(([0-9]+)\\) ([0-9a-f]+) .*"
		line :extended t)
	    (when match?
	      (let ((rw (if (string= (match-string line srw) "READ") :read :write))
		    (addr (- (hexparse (match-string line sAddr)) offset))
		    (len (parse-integer (match-string line sLen) :radix 10))
		    (sVal (hexparse (match-string line sVal))))
		(when (<= 0 addr (1- size))
		  ;; Statistics
		  (format t "~5A ~8X <-> ~8X | ~A~%" rw addr sVal len)
		  )))))))

;;; EOF
