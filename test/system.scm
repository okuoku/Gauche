;;
;; test for system related procedures
;;

(use gauche.test)
(use srfi-1)
(use srfi-13)

(test-start "system")

;;-------------------------------------------------------------------
(test-section "system")
;; test this first, so that we can use system commands to verify our results.

(test "system" #t (lambda () (sys-system ":") #t))
(test "system" #t (lambda () (sys-system "") #t))

(define (get-command-output command)
  (sys-system "rm -rf test.out")
  (sys-system (format #f "~a > test.out" command))
  (call-with-input-file "test.out"
    (lambda (in)
      (let loop ((line (read-line in)) (lines '()))
        (if (eof-object? line)
            (begin (sys-system "rm -rf test.out")
                   (string-join (reverse lines) " "))
            (loop (read-line in) (cons line lines)))))))

(define (get-lsmode file)
  (string-take (get-command-output (format #f "ls -ld ~a" file)) 10))

(define (get-pwd-via-pwd)
  ;; use pwd command to get pwd.  avoid using shell's built-in pwd,
  ;; for it may be confused by symlinks.
  (cond ((sys-access "/bin/pwd" x_ok) (get-command-output "/bin/pwd"))
        ((sys-access "/usr/bin/pwd" x_ok) (get-command-output "/usr/bin/pwd"))
        ((sys-access "/sbin/pwd" x_ok) (get-command-output "/sbin/pwd"))
        (else (get-command-output "pwd"))))

;;-------------------------------------------------------------------
(test-section "environment")

(test "getenv" (get-command-output "echo $PATH")
      (lambda () (sys-getenv "PATH")))

(test "getenv"
      (let ((x (get-command-output "echo $NoSucHEniIRoNmenT")))
        (if (string-null? x) #f x))
      (lambda () (sys-getenv "NoSucHEniIRoNmenT")))

(test "getcwd" (get-pwd-via-pwd)
      (lambda () (sys-getcwd)))

;;-------------------------------------------------------------------
(test-section "pathnames")

(test "basename" "ghi.jkl" (lambda () (sys-basename "/abc/def/ghi.jkl")))
(test "dirname" "/abc/def" (lambda () (sys-dirname "/abc/def/ghi.jkl")))
(test "basename" "ghi.jkl" (lambda () (sys-basename "/abc/def/ghi.jkl/")))
(test "dirname" "/abc/def" (lambda () (sys-dirname "/abc/def/ghi.jkl/")))
(test "basename" "ghi.jkl" (lambda () (sys-basename "/abc//def//ghi.jkl//")))
(test "dirname" "/abc//def" (lambda () (sys-dirname "/abc//def//ghi.jkl//")))
(test "basename" "ghi.jkl" (lambda () (sys-basename "ghi.jkl")))
(test "dirname" "." (lambda () (sys-dirname "ghi.jkl")))

(test "basename" "" (lambda () (sys-basename "")))
(test "dirname"  "." (lambda () (sys-dirname "")))
(test "basename" "" (lambda () (sys-basename "/")))
(test "dirname"  "/" (lambda () (sys-dirname "/")))
(test "basename" "" (lambda () (sys-basename "//")))
(test "dirname"  "/" (lambda () (sys-dirname "//")))

(test "basename" ".." (lambda () (sys-basename "../")))
(test "dirname"  "." (lambda () (sys-dirname "../")))
(test "basename" ".." (lambda () (sys-basename "../..")))
(test "dirname"  ".." (lambda () (sys-dirname "../..")))

(test "normalize" (string-append (get-pwd-via-pwd) "/.")
      (lambda () (sys-normalize-pathname "." :absolute #t)))
(test "normalize" (string-append (get-pwd-via-pwd) "/")
      (lambda () (sys-normalize-pathname "" :absolute #t)))
(test "normalize" (string-append (get-command-output "echo $HOME") "/abc")
      (lambda () (sys-normalize-pathname "~/abc" :expand #t)))
(test "normalize" "/a/b/c/d/e"
      (lambda () (sys-normalize-pathname "/a/b//.///c//d/./e"
                                         :canonicalize #t)))
(test "normalize" "/a/b/c/d/e/"
      (lambda () (sys-normalize-pathname "/a/b//.///c//d/./e/"
                                         :canonicalize #t)))
(test "normalize" "/a/b/c/d/e/"
      (lambda () (sys-normalize-pathname "/a/B//./../c/d/../../b//c/d/e/f/.."
                                         :canonicalize #t)))
(test "normalize" ""
      (lambda () (sys-normalize-pathname ""
                                         :canonicalize #t)))
(test "normalize" "a/../../.."
      (lambda () (sys-normalize-pathname "a/b/c/../../../../.."
                                         :canonicalize #t)))

;;-------------------------------------------------------------------
(test-section "filesystem")

(sys-system "rm -rf test.dir >/dev/null")

(test "access" '(#f #f #f #f)
      (lambda ()
        (map (lambda (flag) (sys-access "test.dir" flag))
             (list f_ok r_ok w_ok x_ok))))
(sys-system "touch test.dir")
(sys-system "chmod 777 test.dir")
(test "access" '(#t #t #t #t)
      (lambda ()
        (map (lambda (flag) (sys-access "test.dir" flag))
             (list f_ok r_ok w_ok x_ok))))
(sys-system "chmod 500 test.dir")
(test "access" '(#t #t #f #t)
      (lambda ()
        (map (lambda (flag) (sys-access "test.dir" flag))
             (list f_ok r_ok w_ok x_ok))))
(sys-system "chmod 477 test.dir")
(test "access" '(#t #t #f #f)
      (lambda ()
        (map (lambda (flag) (sys-access "test.dir" flag))
             (list f_ok r_ok w_ok x_ok))))
(sys-system "chmod 000 test.dir")
(test "access" '(#t #f #f #f)
      (lambda ()
        (map (lambda (flag) (sys-access "test.dir" flag))
             (list f_ok r_ok w_ok x_ok))))

(test "unlink" #f
      (lambda ()
        (sys-unlink "test.dir") (sys-access "test.dir" f_ok)))

(test "mkdir" "drwxr-x---"
      (lambda ()
        (sys-mkdir "test.dir" #o750)
        (get-lsmode "test.dir")))

(test "chmod" "drwxr-xr-x"
      (lambda ()
        (sys-chmod "test.dir" #o755)
        (get-lsmode "test.dir")))

(with-output-to-file "test.dir/xyzzy"
  (lambda () (display "zzzzzZzzzzZZzZzzzzzzzZzzZZZZz") (newline)))

(test "rename" '(#f #t)
      (lambda ()
        (sys-rename "test.dir/xyzzy" "test.dir/zzZzz")
        (list (sys-access "test.dir/xyzzy" f_ok)
              (sys-access "test.dir/zzZzz" f_ok))))

(test "readdir" '("." ".." "zzZzz")
      (lambda ()
        (sort (sys-readdir "test.dir"))))

(test "link" '("." ".." "xyzzy" "zzZzz")
      (lambda ()
        (sys-link "test.dir/zzZzz" "test.dir/xyzzy")
        (sort (sys-readdir "test.dir"))))

(test "unlink" '("." ".." "xyzzy")
      (lambda ()
        (sys-unlink "test.dir/zzZzz")
        (sort (sys-readdir "test.dir"))))

(test "rename" '("." ".." "zzZzz")
      (lambda ()
        (sys-rename "test.dir/xyzzy" "test.dir/zzZzz")
        (sort (sys-readdir "test.dir"))))

(test "rmdir" #f
      (lambda ()
        (sys-unlink "test.dir/zzZzz")
        (sys-rmdir "test.dir")
        (sys-access "test.dir" f_ok)))

;;-------------------------------------------------------------------
(test-section "stat")

(sys-system "rm -rf test.dir > /dev/null")
(with-output-to-file "test.dir" (lambda () (display "01234")))
(sys-chmod "test.dir" #o654)

(test "stat" '(#o654 regular 5)
      (lambda ()
        (let ((s (sys-stat "test.dir")))
          (list (logand #o777 (sys-stat->mode s))
                (sys-stat->file-type s)
                (sys-stat->size s)))))

(test "fstat" '(#o654 regular 5)
      (lambda ()
        (call-with-input-file "test.dir"
          (lambda (p)
            (let ((s (sys-fstat p)))
              (list (logand #o777 (sys-stat->mode s))
                    (sys-stat->file-type s)
                    (sys-stat->size s)))))))

(sys-unlink "test.dir")
(sys-mkdir "test.dir" #o700)

(test "stat" '(#o700 directory)
      (lambda ()
        (let ((s (sys-stat "test.dir")))
          (list (logand #o777 (sys-stat->mode s))
                (sys-stat->file-type s)))))

(test "fstat" '(#o700 directory)
      (lambda ()
        (call-with-input-file "test.dir"
          (lambda (p)
            (let ((s (sys-fstat p)))
              (list (logand #o777 (sys-stat->mode s))
                    (sys-stat->file-type s)))))))

(sys-remove "test.dir")

;;-------------------------------------------------------------------
(test-section "fork&exec")

(test "fork & wait" #t
      (lambda ()
        (let ((pid (sys-fork)))
          (if (= pid 0)
              (sys-exit 5)
              (let ((status (sys-wait)))
                (and (= (car status) pid)
                     (sys-wait-exited? (cdr status))
                     (= (sys-wait-exit-status (cdr status)) 5)))))))

(test "fork & waitpid" #t
      (lambda ()
        (let ((pid (sys-fork)))
          (if (= pid 0)
              (sys-exit 10)
              (let ((status (sys-waitpid pid)))
                (and (= (car status) pid)
                     (sys-wait-exited? (cdr status))
                     (= (sys-wait-exit-status (cdr status)) 10)))))))

(test "fork, wait & kill" #t
      (lambda ()
        (let ((pid (sys-fork)))
          (if (= pid 0)
              (begin (sys-pause) (sys-exit 0))
              (begin 
                (sys-kill pid SIGINT)
                (let ((status (sys-wait)))
                  (and (= (car status) pid)
                       (sys-wait-signaled? (cdr status))
                       (= (sys-wait-termsig (cdr status)) SIGINT))))))))

(test "fork, wait, kill & sleep" #t
      (lambda ()
        (let ((pid (sys-fork)))
          (if (= pid 0)
              (begin (sys-sleep 1) (sys-exit 0))
              (begin 
                (sys-kill pid SIGSTOP) 
                (let ((status (sys-waitpid pid :untraced #t)))
                  (and (= (car status) pid)
                       (sys-wait-stopped? (cdr status))
                       (= (sys-wait-stopsig (cdr status)) SIGSTOP)
                       (begin (sys-kill pid SIGCONT)
                              (let ((status (sys-wait)))
                                (and (= (car status) pid)
                                     (sys-wait-exited? (cdr status))
                                     (= (sys-wait-exit-status (cdr status)) 0)
                                     )))
                       )))
              )))
      )

;;-------------------------------------------------------------------
(test-section "select")

(test "fdset" '(3 #t #f #t #t #f)
      (lambda ()
        (let ((fdset (make <sys-fdset>)))
          (set! (sys-fdset-ref fdset (current-input-port)) #t)
          (sys-fdset-set! fdset (current-error-port) #t)
          (sys-fdset-set! fdset 3 #t)
          (sys-fdset-set! fdset 4 #f)
          (cons (sys-fdset-max-fd fdset)
                (map (lambda (i) (sys-fdset-ref fdset i)) (iota 5))))))

(test "fdset" '(-1 7 7 4 10 10 -1)
      (lambda ()
        (let ((fdset (make <sys-fdset>))
              (result '()))
          (define (push-result)
            (set! result (cons (sys-fdset-max-fd fdset) result)))
          (push-result)
          (sys-fdset-set! fdset 7 #t)
          (push-result)
          (sys-fdset-set! fdset 4 #t)
          (push-result)
          (sys-fdset-set! fdset 7 #f)
          (push-result)
          (sys-fdset-set! fdset 10 #t)
          (push-result)
          (sys-fdset-set! fdset 4 #f)
          (push-result)
          (sys-fdset-set! fdset 10 #f)
          (push-result)
          (reverse result))))

(test "select" '(0 #f #f #f #f 1 #t #f #f #t #\x)
      (lambda ()
        (let* ((pipe (sys-pipe))
               (pid (sys-fork)))
          (if (= pid 0)
              (begin (sys-select #f #f #f 100000)
                     (display "x" (cadr pipe))
                     (close-output-port (cadr pipe))
                     (sys-exit 0))
              (let ((rfds (make <sys-fdset>)))
                (sys-fdset-set! rfds (car pipe) #t)
                (receive (an ar aw ae)
                    (sys-select rfds #f #f 0)
                  (receive (bn br bw be)
                      (sys-select! rfds #f #f #f)
                    (list an (eq? ar rfds) aw ae
                          (sys-fdset-ref ar (car pipe))
                          bn (eq? br rfds) bw be
                          (sys-fdset-ref rfds (car pipe))
                          (read-char (car pipe))))))))))

(test-end)

