(defun update-nul-libvirt-server-cc ()
  "When called from nova-types.h it updates array of strings
strings in server.cc to match the list defined errors"
  (save-excursion
    (beginning-of-buffer)
    (let* ((beg (progn
		  (re-search-forward "^enum LIBVIRT_NOVA_OPCODE \{")
		  (beginning-of-line 2)
		  (point)))
	   (end (progn
		  (search-forward "}")
		  (beginning-of-line)
		  (point))))
      (let ((opcodes))
	(save-restriction
	  (narrow-to-region beg end)
	  (goto-char (point-min))
	  (while (re-search-forward "^[[:space:]]*[^[:space]]*" nil t)
	    (prin1 opcodes)
	    (push (current-word) opcodes)))
	(setq opcodes (reverse opcodes))
	(save-current-buffer
	  (set-buffer (find-file-noselect "server.cc"))
	  (beginning-of-buffer)
	  (search-forward "const char *op2string(unsigned op)")
	  (search-forward "switch (op) {")
	  (kill-region (point) (save-excursion (search-forward "}") (backward-char) (point)))
	  (insert "\n")
	  (dolist (op opcodes)
	    (insert (concat "case " op ": return \"" op "\";"))
	    (indent-according-to-mode)
	    (insert "\n")
	    (indent-according-to-mode)))))
    ;; (message "server.cc updated")
    ))
