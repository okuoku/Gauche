;;;
;;; cgi.scm - CGI utility
;;;
;;;  Copyright(C) 2001 by Shiro Kawai (shiro@acm.org)
;;;
;;;  Permission to use, copy, modify, distribute this software and
;;;  accompanying documentation for any purpose is hereby granted,
;;;  provided that existing copyright notices are retained in all
;;;  copies and that this notice is included verbatim in all
;;;  distributions.
;;;  This software is provided as is, without express or implied
;;;  warranty.  In no circumstances the author(s) shall be liable
;;;  for any damages arising out of the use of this software.
;;;
;;;  $Id: cgi.scm,v 1.5 2001-11-12 08:24:44 shirok Exp $
;;;

;; Surprisingly, there's no ``formal'' definition of CG.
;; The most reliable document I found is in <http://CGI-Spec.Golux.Com/>

(define-module www.cgi
  (use srfi-1)
  (use srfi-13)
  (use rfc.uri)
  (use rfc.cookie)
  (use text.tree)
  (use text.html-lite)
  (export cgi-parse-parameters
          cgi-get-parameter
          cgi-header
          cgi-main)
  )
(select-module www.cgi)

(define (cgi-get-query)
  (let ((method (sys-getenv "REQUEST_METHOD")))
    (cond ((not method)  ;; interactive use.
           (if (sys-isatty (current-input-port))
               (begin
                 (display "Enter parameters (name=value).  ^D to stop.\n")
                 (flush)
                 (let loop ((line (read-line))
                            (params '()))
                   (if (eof-object? line)
                       (string-join (reverse params) "&")
                       (loop (read-line) (cons line params)))))
               (error "REQUEST_METHOD not defined")))
          ((or (string-ci=? method "GET")
               (string-ci=? method "HEAD"))
           (or (sys-getenv "QUERY_STRING") ""))
          ((string-ci=? method "POST")
           ;; TODO: mutipart message
           (string-concatenate (port->string-list (current-input-port))))
          (else (error "unknown REQUEST_METHOD" method)))))

(define (cgi-parse-parameters . args)
  (let ((input   (or (get-keyword :query-string args #f)
                     (cgi-get-query)))
        (cookies (cond ((and (get-keyword :merge-cookies args #f)
                             (sys-getenv "HTTP_COOKIE"))
                        => parse-cookie-string)
                       (else '()))))
    (append
     (cond
      ((string-null? input) '())
      (else
       (fold-right (lambda (elt params)
                     (let* ((ss (string-split elt #\=))
                            (p  (assoc (car ss) params))
                            (v  (if (null? (cdr ss))
                                    #t
                                    (uri-decode-string (string-join (cdr ss) "=")
                                                       :cgi-decode #t))))
                       (if p
                           (begin (set! (cdr p) (cons v (cdr p))) params)
                           (cons (list (car ss) v) params))))
                   '()
                   (string-split input #\&))))
     (map (lambda (cookie) (list (car cookie) (cadr cookie))) cookies))))

(define (cgi-get-parameter key params . args)
  (let* ((list?   (get-keyword :list args #f))
         (default (get-keyword :default args (if list? '() #f)))
         (convert (get-keyword :convert args (lambda (x) x))))
    (cond ((assoc key params)
           => (lambda (p)
                (if list?
                    (map convert (cdr p))
                    (convert (cadr p)))))
          (else default))))

(define (cgi-header . args)
  (let ((content-type (get-keyword :content-type args "text/html"))
        (location     (get-keyword :location args #f))
        (cookies      (get-keyword :cookies args '())))
    (if location
        (list "Location: " (x->string location) "\n\n")
        (list "Content-type: " (x->string content-type) "\n"
              (map (lambda (cookie)
                     (list "Set-cookie: " cookie "\n"))
                   cookies)
              "\n"))))

(define (cgi-main proc . args)
  (let ((eproc (get-keyword :on-error args
                            (lambda (e)
                              `(,(cgi-header)
                                ,(html-html
                                  (html-head (html-title "Error"))
                                  (html-body (html-h1 "Error")
                                             (html-p (html-escape-string
                                                      (slot-ref e 'message))))
                                  )))))
        (params (cgi-parse-parameters :merge-cookies
                                      (get-keyword :merge-cookies args #f))))
    (write-tree (with-error-handler eproc (lambda () (proc params))))))

(provide "www/cgi")
