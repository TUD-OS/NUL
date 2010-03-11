#!/usr/bin/clisp -q -norc
;;; -*- Mode: Lisp -*-

(defpackage :js.tools.rw-log
  (:use :common-lisp :regexp))
(in-package :js.tools.rw-log)

;;; Usage: rw-log.lisp 0xF7CE0000 0x4000 log.txt

(defparameter *names* '((#x1520 "VTEICS")
			(#x1524 "VTEIMS")
			(#x152C "VTEIAC")
			(#x1530 "VTEIAM")
			(#x2818 "RDT0")
			(#xC40  "VMMB")
			(#x0    "VTCTRL")
			(#x8    "VTSTATUS")))

(defun hexparse (s)
  (values
   (parse-integer
    s :radix 16
    :start (if (string= "0x" s :end2 (min (length s) 2))
	       2 0))))

(defun sort-accesses (hash)
  (let ((l ()))
    (maphash (lambda (offset count)
	       (push (list offset count) l))
	     hash)
    (sort l #'> :key #'second)))

(defun print-stats (name hash)
  (let ((l (sort-accesses hash)))
    (format t "~A:~%" name)
    (loop 
       for (offset count) in l
       do (format t "~10A ~4X: ~4A times~%" (or (second (assoc offset *names*)) "?") 
		  offset count))
    (terpri)))

(let ((offset (hexparse (first ext:*args*)))
      (size (hexparse (second ext:*args*)))
      (reads (make-hash-table))
      (writes (make-hash-table)))
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
		  ;; Log
		  ;(format t "~5A ~4X <-> ~8X | ~A~%" rw addr sVal len)
		  (incf (gethash addr (ecase rw (:read reads) (:write writes)) 0))
		  ))))))
  (print-stats "READ" reads)
  (print-stats "WRITE" writes)

  )

;;; EOF
