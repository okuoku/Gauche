;;;
;;; condutil.scm - condition primitives.  autoloaded.
;;;  
;;;   Copyright (c) 2004 Shiro Kawai, All rights reserved.
;;;   
;;;   Redistribution and use in source and binary forms, with or without
;;;   modification, are permitted provided that the following conditions
;;;   are met:
;;;   
;;;   1. Redistributions of source code must retain the above copyright
;;;      notice, this list of conditions and the following disclaimer.
;;;  
;;;   2. Redistributions in binary form must reproduce the above copyright
;;;      notice, this list of conditions and the following disclaimer in the
;;;      documentation and/or other materials provided with the distribution.
;;;  
;;;   3. Neither the name of the authors nor the names of its contributors
;;;      may be used to endorse or promote products derived from this
;;;      software without specific prior written permission.
;;;  
;;;   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
;;;   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
;;;   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
;;;   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
;;;   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
;;;   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
;;;   TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
;;;   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
;;;   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
;;;   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
;;;   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
;;;  
;;;  $Id: condutil.scm,v 1.3 2004-10-11 10:30:13 shirok Exp $
;;;

;; Defines some condition-related primitives.

(define-module gauche.condutil
  (export make-condition-type condition-type?
          make-condition condition-ref extract-condition
          define-condition-type condition)
  )
(select-module gauche.condutil)

;; defined in C:
;;   condition?
;;   condition-has-type?
;;   make-compound-condition

(define (make-condition-type name parent field-names)
  (unless (condition-type? parent)
    (error "condition-type required as a parent of make-condition-type, but got:" parent))
  (make <condition-meta>
    :name name
    :supers (list parent)
    :slots  field-names))

(define (condition-type? obj)
  (is-a? obj <condition-meta>))

(define (make-condition type . initargs)
  (unless (condition-type? type)
    (error "make-condition requires a condition type, but got:" type))
  (let ((c (make type)))
    (let loop ((args initargs))
      (cond ((null? args) c)
            ((null? (cdr args))
             (error "make-condition is given non-even initargs:" initargs))
            ((slot-exists? c (car args))
             (slot-set! c (car args) (cadr args))
             (loop (cddr args)))
            (else
             (errorf "condition type ~s doesn't have a field ~s"
                     type (car args)))))))

(define (condition-ref c slot)
  (slot-ref c slot))  ;; compound condition traps slot-missing

(define (extract-condition c type)
  (unless (condition-has-type? c type)
    (error "cannot extract a condition of type ~s from a condition ~s"
           type c))
  (let ((cc (make type)))
    (let loop ((slots (class-slots type)))
      (if (null? slots)
        cc
        (let ((sn (slot-definition-name (car slots))))
          (slot-set! cc sn (slot-ref c sn))
          (loop (cdr slots)))))))

;; macros

(define-syntax define-condition-type
  (syntax-rules ()
    ((define-condition-type name super pred . field-spec)
     (begin
       (define-condition-type-rec name super () field-spec)
       (define (pred obj) (condition-has-type? obj name))))
    ((_ . other)
     (syntax-error "malformed define-condition-type:"
                   (define-condition-type . other)))))

(define-syntax define-condition-type-rec
  (syntax-rules ()
    ((define-condition-type-rec name super slots ())
     (define-class name (super) slots :metaclass <condition-meta>))
    ((define-condition-type-rec name super (slot ...)
       ((field reader) . more-fields))
     (begin
       (define (reader obj) (condition-ref obj 'field))
       (define-condition-type-rec name super (slot ... field)
         more-fields)))
    ((_ name super slots (badfield . more))
     (syntax-error "bad field spec for define-condition-type:" badfield))
    ))

(define-syntax condition
  (syntax-rules ()
    ((condition (type . bindings) ...)
     (make-compound-condition
      (condition-sub type () bindings) ...))
    ((_ . other)
     (syntax-error "malformed condition:" (condition . other)))))

(define-syntax condition-sub
  (syntax-rules ()
    ((condition-sub type inits ())
     (make-condition type . inits))
    ((condition-sub type (init ...) ((field expr) . more))
     (condition-sub type (init ... 'field expr) more))
    ((condition-sub type inits (other . more))
     (syntax-error "malformed condition field initializer:" other))))

;; A trick to allow slot-ref to be used for compound condition.
(define-method slot-missing ((class <condition-meta>)
                             (cc <compound-condition>)
                             slot)
  (let loop ((members (ref cc '%conditions)))
    (cond ((null? members) (slot-missing class cc slot))
          ((slot-exists? (car members) slot)
           (slot-ref (car members) slot))
          (else (loop (cdr members))))))

;; A trick to let a condition type behave its own predicate
(define-method object-apply ((type <condition-meta>) obj)
  (condition-has-type? obj type))

(provide "gauche/condutil")
