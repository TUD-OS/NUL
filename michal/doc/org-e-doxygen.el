;;; org-e-doxygen.el --- Doxygen Back-End For Org Export Engine

;; Copyright (C) 2011-2012  Free Software Foundation, Inc.

;; Author: Nicolas Goaziou <n.goaziou at gmail dot com>
;; Keywords: outlines, hypermedia, calendar, wp

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:

;; This library implements a Doxygen back-end for Org generic exporter.

;; To test it, run
;;
;;   M-: (org-export-to-buffer 'e-doxygen "*Test e-Doxygen*") RET
;;
;; in an org-mode buffer then switch to the buffer to see the Doxygen
;; export.  See contrib/lisp/org-export.el for more details on how
;; this exporter works.

;; It introduces three new buffer keywords: "DOXYGEN_CLASS",
;; "DOXYGEN_CLASS_OPTIONS" and "DOXYGEN_HEADER".

;;; Code:

;;; org-e-doxygen.el

;;; Template

(defun org-e-doxygen-template (contents info)
  "Return complete document string after Doxygen conversion.
CONTENTS is the transcoded contents string.  INFO is a plist
holding export options."
  (let ((input-file (plist-get info :input-file)))
    (format "/* This file was automatically created from %s */\n/**\n@mainpage\n%s\n*/\n"
	    input-file contents)))



;;; Transcode Functions

;;;; Block

(defun org-e-doxygen-center-block (center-block contents info)
  "Transcode a CENTER-BLOCK element from Org to Doxygen.
CONTENTS holds the contents of the block.  INFO is a plist
holding contextual information."
  (format "<center>\n%s</center>" contents))


;;;; Comment

;; Comments are ignored.


;;;; Comment Block

;; Comment Blocks are ignored.


;;;; Drawer

(defun org-e-doxygen-drawer (drawer contents info)
  "Transcode a DRAWER element from Org to Doxygen.
CONTENTS holds the contents of the block.  INFO is a plist
holding contextual information."
  (let* ((name (org-element-property :drawer-name drawer))
	 (output (if (functionp org-e-doxygen-format-drawer-function)
		     (funcall org-e-doxygen-format-drawer-function
			      name contents)
		   ;; If there's no user defined function: simply
		   ;; display contents of the drawer.
		   contents)))
    (org-e-doxygen--wrap-label drawer output)))


;;;; Dynamic Block

(defun org-e-doxygen-dynamic-block (dynamic-block contents info)
  "Transcode a DYNAMIC-BLOCK element from Org to Doxygen.
CONTENTS holds the contents of the block.  INFO is a plist
holding contextual information.  See
`org-export-data'."
  (org-e-doxygen--wrap-label dynamic-block contents))


;;;; Emphasis

(defcustom org-e-doxygen-emphasis-alist
  '(("*" . "<b>%s</b>")
    ("/" . "<i>%s</i>")
    ("_" . "%s")
    ("+" . "%s")
    ("=" . "<code>%s</code>")
    ("~" . "%s"))
  "Alist of Doxygen expressions to convert emphasis fontifiers.

The key is the character used as a marker for fontification.  The
value is a formatting string to wrap fontified text with."
  :group 'org-export-e-doxygen
  :type 'alist)

(defun org-e-doxygen-emphasis (emphasis contents info)
  "Transcode EMPHASIS from Org to Doxygen.
CONTENTS is the contents of the emphasized text.  INFO is a plist
holding contextual information.."
  (format (cdr (assoc (org-element-property :marker emphasis)
		      org-e-doxygen-emphasis-alist))
	  contents))


;;;; Entity

(defun org-e-doxygen-entity (entity contents info)
  "Transcode an ENTITY object from Org to Doxygen.
CONTENTS are the definition itself.  INFO is a plist holding
contextual information."
  (let ((ent (org-element-property :doxygen entity)))
    (if (org-element-property :doxygen-math-p entity) (format "$%s$" ent) ent)))


;;;; Example Block

;; (defun org-e-doxygen-example-block (example-block contents info)
;;   "Transcode a EXAMPLE-BLOCK element from Org to Doxygen.
;; CONTENTS is nil.  INFO is a plist holding contextual information."
;;   (format "@code\n%s@endcode" (org-element-property :value example-block)))


;;;; Export Snippet

(defun org-e-doxygen-example-block (example-block contents info)
  "Transcode a EXAMPLE-BLOCK element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (let* ((options (or (org-element-property :options example-block) ""))
	 (value (org-export-handle-code example-block info)))
    (format "@verbatim\n%s@endverbatim" value)))

;;;; Export Snippet

(defun org-e-doxygen-export-snippet (export-snippet contents info)
  "Transcode a EXPORT-SNIPPET object from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (when (eq (org-export-snippet-backend export-snippet) 'e-doxygen)
    (org-element-property :value export-snippet)))


;;;; Export Block

(defun org-e-doxygen-export-block (export-block contents info)
  "Transcode a EXPORT-BLOCK element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (when (string= (org-element-property :type export-block) "doxygen")
    (org-remove-indentation (org-element-property :value export-block))))


;;;; Fixed Width

(defun org-e-doxygen-fixed-width (fixed-width contents info)
  "Transcode a FIXED-WIDTH element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (let* ((value (org-element-normalize-string
		 (org-element-property :value fixed-width))))
    (format "@verbatim\n%s@endverbatim" value)))


;;;; Footnote Definition

;; Footnote Definitions are ignored.


;;;; Footnote Reference

(defun org-e-doxygen-footnote-reference (footnote-reference contents info)
  "Transcode a FOOTNOTE-REFERENCE element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (concat
   ;; Insert separator between two footnotes in a row.
   (let ((prev (org-export-get-previous-element footnote-reference info)))
     (when (eq (org-element-type prev) 'footnote-reference)
       org-e-doxygen-footnote-separator))
   (cond
    ;; Use \footnotemark if the footnote has already been defined.
    ((not (org-export-footnote-first-reference-p footnote-reference info))
     (format "\\footnotemark[%s]{}"
	     (org-export-get-footnote-number footnote-reference info)))
    ;; Use also \footnotemark if reference is within another footnote
    ;; reference or footnote definition.
    ((loop for parent in (org-export-get-genealogy footnote-reference info)
	   thereis (memq (org-element-type parent)
			 '(footnote-reference footnote-definition)))
     (let ((num (org-export-get-footnote-number footnote-reference info)))
       (format "\\footnotemark[%s]{}\\setcounter{footnote}{%s}" num num)))
    ;; Otherwise, define it with \footnote command.
    (t
     (let ((def (org-export-get-footnote-definition footnote-reference info)))
       (unless (eq (org-element-type def) 'org-data)
	 (setq def (cons 'org-data (cons nil def))))
       (concat
	(format "\\footnote{%s}" (org-trim (org-export-data def 'e-doxygen info)))
	;; Retrieve all footnote references within the footnote and
	;; add their definition after it, since Doxygen doesn't support
	;; them inside.
	(let (all-refs
	      (search-refs
	       (function
		(lambda (data)
		  ;; Return a list of all footnote references in DATA.
		  (org-element-map
		   data 'footnote-reference
		   (lambda (ref)
		     (when (org-export-footnote-first-reference-p ref info)
		       (push ref all-refs)
		       (when (eq (org-element-property :type ref) 'standard)
			 (funcall
			  search-refs
			  (org-export-get-footnote-definition ref info)))))
		   info) (reverse all-refs)))))
	  (mapconcat
	   (lambda (ref)
	     (format
	      "\\footnotetext[%s]{%s}"
	      (org-export-get-footnote-number ref info)
	      (org-trim
	       (funcall
		(if (eq (org-element-property :type ref) 'inline)
		    'org-export-secondary-string
		  'org-export-data)
		(org-export-get-footnote-definition ref info) 'e-doxygen info))))
	   (funcall search-refs def) ""))))))))


;;;; Headline

(defun org-e-doxygen--headline-id (headline info)
  (let* ((numlist (org-export-get-headline-number headline info))
	 (id (concat "sec_" (mapconcat 'number-to-string numlist "_"))))
    id))

(defun org-e-doxygen-headline (headline contents info)
  "Transcode an HEADLINE element from Org to Doxygen.
CONTENTS holds the contents of the headline.  INFO is a plist
holding contextual information."
					;(message "Headline: %s" (prin1-to-string headline))
  (let* ((numlist (org-export-get-headline-number headline info))
	 (id (org-e-doxygen--headline-id headline info))
	 (number (mapconcat 'number-to-string numlist "."))
	 (title (org-element-property :raw-value headline))
	 (abslevel (org-element-property :level headline))
	 (pagelevel (let ((current (or (car (plist-get info :doxygen_pagelevel))
				       0))
			  (prev (cdr (plist-get info :doxygen_pagelevel))))
		      (if (< abslevel current)
			  (progn
			    (plist-put info :doxygen_pagelevel prev)
			    (car prev))
			current)))
	 (level (- abslevel pagelevel))
	 (seccmd (cond
		  ((eq level 0) "@page")
		  ((eq level 1) "@section")
		  ((eq level 2) "@subsection")
		  ((eq level 3) "@subsubsection")
		  ((eq level 4) "@paragraph")
		  (t (error "Heading level too deep for doxygen")))))
    ;(message "pagelevel@%s %s = %s/%s" number title (prin1-to-string (plist-get info :doxygen_pagelevel)) abslevel)
    (concat (format "%s %s %s %s\n" seccmd id number title)
	    contents)))

;;;; Horizontal Rule

;; (defun org-e-doxygen-horizontal-rule (horizontal-rule contents info)
;;   "Transcode an HORIZONTAL-RULE  object from Org to Doxygen.
;; CONTENTS is nil.  INFO is a plist holding contextual information."
;;   (let ((attr (mapconcat #'identity
;; 			 (org-element-property :attr_doxygen horizontal-rule)
;; 			 " ")))
;;     (org-e-doxygen--wrap-label horizontal-rule (concat "\\hrule " attr))))


;;;; Inline Babel Call

;; Inline Babel Calls are ignored.


;;;; Inline Src Block

(defun org-e-doxygen-inline-src-block (inline-src-block contents info)
  "Transcode an INLINE-SRC-BLOCK element from Org to Doxygen.
CONTENTS holds the contents of the item.  INFO is a plist holding
contextual information."
  (let* ((code (org-element-property :value inline-src-block))
	 (separator (org-e-doxygen--find-verb-separator code)))
    (cond
     ;; Do not use a special package: transcode it verbatim.
     ((not org-e-doxygen-listings)
      (concat "\\verb" separator code separator))
     ;; Use minted package.
     ((eq org-e-doxygen-listings 'minted)
      (let* ((org-lang (org-element-property :language inline-src-block))
	     (mint-lang (or (cadr (assq (intern org-lang)
					org-e-doxygen-minted-langs))
			    org-lang))
	     (options (org-e-doxygen--make-option-string
		       org-e-doxygen-minted-options)))
	(concat (format "\\mint%s{%s}"
			(if (string= options "") "" (format "[%s]" options))
			mint-lang)
		separator code separator)))
     ;; Use listings package.
     (t
      ;; Maybe translate language's name.
      (let* ((org-lang (org-element-property :language inline-src-block))
	     (lst-lang (or (cadr (assq (intern org-lang)
				       org-e-doxygen-listings-langs))
			   org-lang))
	     (options (org-e-doxygen--make-option-string
		       (append org-e-doxygen-listings-options
			       `(("language" ,lst-lang))))))
	(concat (format "\\lstinline[%s]" options)
		separator code separator))))))


;;;; Inlinetask

(defun org-e-doxygen-inlinetask (inlinetask contents info)
  "Transcode an INLINETASK element from Org to Doxygen.
CONTENTS holds the contents of the block.  INFO is a plist
holding contextual information."
  (let ((title (org-export-secondary-string
	       (org-element-property :title inlinetask) 'e-doxygen info))
	(todo (and (plist-get info :with-todo-keywords)
		   (let ((todo (org-element-property
				:todo-keyword inlinetask)))
		     (and todo
			  (org-export-secondary-string todo 'e-doxygen info)))))
	(todo-type (org-element-property :todo-type inlinetask))
	(tags (and (plist-get info :with-tags)
		   (org-element-property :tags inlinetask)))
	(priority (and (plist-get info :with-priority)
		       (org-element-property :priority inlinetask))))
    ;; If `org-e-doxygen-format-inlinetask-function' is provided, call it
    ;; with appropriate arguments.
    (if (functionp org-e-doxygen-format-inlinetask-function)
	(funcall org-e-doxygen-format-inlinetask-function
		 todo todo-type priority title tags contents)
      ;; Otherwise, use a default template.
      (org-e-doxygen--wrap-label
       inlinetask
       (let ((full-title
	      (concat
	       (when todo (format "\\textbf{\\textsf{\\textsc{%s}}} " todo))
	       (when priority (format "\\framebox{\\#%c} " priority))
	       title
	       (when tags (format "\\hfill{}\\textsc{%s}" tags)))))
	 (format (concat "\\begin{center}\n"
			 "\\fbox{\n"
			 "\\begin{minipage}[c]{.6\\textwidth}\n"
			 "%s\n\n"
			 "\\rule[.8em]{\\textwidth}{2pt}\n\n"
			 "%s"
			 "\\end{minipage}\n"
			 "}\n"
			 "\\end{center}")
		 full-title contents))))))


;;;; Item

(defun org-e-doxygen-item (item contents info)
  "Transcode an ITEM element from Org to Doxygen.
CONTENTS holds the contents of the item.  INFO is a plist holding
contextual information."
  ;; Grab `:level' from plain-list properties, which is always the
  ;; first element above current item.
  (let* ((level (org-element-property :level (org-export-get-parent item info)))
	 (indent (make-string (* 2 level) ?\s)) ; spaces
	 (type (org-element-property :type (org-export-get-parent item info)))
	 (tag (let ((tag (org-element-property :tag item)))
		(and tag
		     (org-export-secondary-string tag 'e-doxygen info))))
	 (formatstr (cond
		     ((eq type 'ordered) "-# %s")
		     ((eq type 'unordered) "- %s")
		     ((eq type 'descriptive) (format "<dt>%s</dt><dd>%%s</dd>" tag)))))
    (concat indent (format formatstr contents))))


;;;; Keyword

(defun org-e-doxygen-keyword (keyword contents info)
  "Transcode a KEYWORD element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  ;;(message "Keyword: %s" (prin1-to-string keyword)))
  (let ((key (downcase (org-element-property :key keyword)))
	(value (org-element-property :value keyword)))
    (cond
     ((string= key "doxygen") value)
     ((string= key "doxygen_subpages")
      (let* ((parent-headline (or (org-export-get-parent-headline keyword info)
				  (plist-get info :parse-tree)))
	     (parent-level (or (org-element-property :level parent-headline)
			       0))
	     (pagelevel (plist-get info :doxygen_pagelevel)))
	(plist-put info :doxygen_pagelevel (push (1+ parent-level) pagelevel))
	(mapconcat 'identity
		   (org-element-map parent-headline 'headline
		    (lambda (headline)
		      (let ((level (org-element-property :level headline)))
			(when (equal level (1+ parent-level))
			  (format "- @subpage %s\n" (org-e-doxygen--headline-id headline info)))))
		    info)
		   ""))))))


;;;; Latex Environment

(defun org-e-doxygen-latex-environment (latex-environment contents info)
  "Transcode a LATEX-ENVIRONMENT element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (let ((label (org-element-property :name latex-environment))
	(value (org-remove-indentation
		(org-element-property :value latex-environment))))
    (if (not (org-string-nw-p label)) value
      ;; Environment is labelled: label must be within the environment
      ;; (otherwise, a reference pointing to that element will count
      ;; the section instead).
      (with-temp-buffer
	(insert value)
	(goto-char (point-min))
	(forward-line)
	(insert (format "\\label{%s}\n" label))
	(buffer-string)))))


;;;; Latex Fragment

(defun org-e-doxygen-latex-fragment (latex-fragment contents info)
  "Transcode a LATEX-FRAGMENT object from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (org-element-property :value latex-fragment))


;;;; Line Break

(defun org-e-doxygen-line-break (line-break contents info)
  "Transcode a LINE-BREAK object from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  "\\\\")


;;;; Link

(defun org-e-doxygen-link (link desc info)
  "Transcode a LINK object from Org to Doxygen.

DESC is the description part of the link, or the empty string.
INFO is a plist holding contextual information.  See
`org-export-data'."
  (let* ((type (org-element-property :type link))
	 (raw-link (org-element-property :raw-link link))
	 (raw-path (org-element-property :path link))
	 ;; Ensure DESC really exists, or set it to nil.
	 (desc (and (not (string= desc "")) desc))
	 (path (cond
		((member type '("http" "https" "ftp" "mailto"))
		 (concat type ":" raw-path))
		((string= type "file")
		 (when (string-match "\\(.+\\)::.+" raw-path)
		   (setq raw-path (match-string 1 raw-path)))
		 (if (file-name-absolute-p raw-path)
		     (concat "file://" (expand-file-name raw-path))
		   ;; TODO: Not implemented yet.  Concat also:
		   ;; (org-export-directory :Doxygen info)
		   (concat "file://" raw-path)))
		(t raw-path)))
	 protocol)
    ;(message (mapconcat (lambda (sym) (format "%s=%s" sym (prin1-to-string (eval sym)))) (list 'type 'raw-link 'raw-path 'desc 'path) " "))
    (cond
     ;; Ref link: If no description is provided, reference label PATH
     ;; and display table number.  Otherwise move to label but display
     ;; description instead.
     ((string= type "ref")
      (if (not desc) (format "@ref %s" path)
	(format "@ref %s \"%s\"" path desc)))
     ;; Links pointing to an headline: Find destination and build
     ;; appropriate referencing command.
     ((member type '("custom-id" "fuzzy" "id"))
      (let ((destination (if (string= type "fuzzy")
			     (org-export-resolve-fuzzy-link link info)
			   (org-export-resolve-id-link link info))))
	;; Fuzzy link points to a target.  Do as above.
	(case (org-element-type destination)
	  (target
	   (format "@ref %s \"%s\""
		   (org-export-solidify-link-text
		    (org-element-property :value destination))
		   (or desc
		       (org-export-secondary-string
			(org-element-property :raw-link link)
			'e-doxygen info))))
	  ;; Fuzzy link points to an headline.  If headlines are
	  ;; numbered and the link has no description, display
	  ;; headline's number.  Otherwise, display description or
	  ;; headline's title.
	  (headline
	   (let ((label
		  (format "sec_%s"
			  (mapconcat
			   'number-to-string
			   (org-export-get-headline-number destination info)
			   "_"))))
	     (if (and (plist-get info :section-numbers) (not desc))
		 (format "@ref %s" label)
	       (format "@ref %s \"%s\"" label
		       (or desc
			   (org-export-secondary-string
			    (org-element-property :title destination)
			    'e-doxygen info))))))
	  ;; Fuzzy link points nowhere.
	  (otherwise
	   (format "%s"
		   (or desc
		       (org-export-secondary-string
			(org-element-property :raw-link link)
			'e-doxygen info)))))))
     ;; Coderef: replace link with the reference name or the
     ;; equivalent line number.
     ((string= type "coderef")
      (format (org-export-get-coderef-format path (or desc ""))
	      (org-export-resolve-coderef path info)))
     ;; Link type is handled by a special function.
     ((functionp (setq protocol (nth 2 (assoc type org-link-protocols))))
      (funcall protocol (org-link-unescape path) desc 'doxygen))
     ;; External link with a description part.
     ((and path desc) (cond
		       ((string= type "file") (format "@image html %s \"%s\"" raw-path desc))
		       (t (format "<a href=\"%s\">%s</a>" path desc))))
     ;; External link without a description part.
     (path (cond
	    ((string= type "file") (format "@image html %s" raw-path))
	    (t (format "<a href=\"%s\">%s</a>" path path))))
     ;; No path, only description.  Try to do something useful.
     (t desc))))


;;;; Babel Call

;; Babel Calls are ignored.


;;;; Macro

(defun org-e-doxygen-macro (macro contents info)
  "Transcode a MACRO element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  ;; Use available tools.
  (org-export-expand-macro macro info))


;;;; Paragraph

(defun org-e-doxygen-paragraph (paragraph contents info)
  "Transcode a PARAGRAPH element from Org to Doxygen.
CONTENTS is the contents of the paragraph, as a string.  INFO is
the plist used as a communication channel."
  contents)


;;;; Plain List

(defun org-e-doxygen-plain-list (plain-list contents info)
  "Transcode a PLAIN-LIST element from Org to Doxygen.
CONTENTS is the contents of the list.  INFO is a plist holding
contextual information."
  (let* ((type (org-element-property :type plain-list))
	 (formatstr (cond
		     ((eq type 'descriptive) "<dl>\n%s</dl>")
		     (t "%s\n"))))
    (format formatstr contents)))


;;;; Plain Text

(defun org-e-doxygen-plain-text (text info)
  "Transcode a TEXT string from Org to Doxygen.
TEXT is the string to transcode.  INFO is a plist holding
contextual information."
  ;; Protect @, &, $, #, %.
  (while (string-match "\\([^\\]\\|^\\)\\([@&$#%]\\)" text)
    (setq text
	  (replace-match (format "\\%s" (match-string 2 text)) nil t text 2)))
  ;; Protect \
  (setq text (replace-regexp-in-string
	      "\\(?:[^\\]\\|^\\)\\(\\\\\\)\\(?:[^@&$#%\\]\\|$\\)"
	      "\\\\" text nil t 1))
  ;; Handle break preservation if required.
  (when (plist-get info :preserve-breaks)
    (setq text (replace-regexp-in-string "\\(\\\\\\\\\\)?[ \t]*\n" " \\\\\\\\\n"
					 text)))
  ;; Return value.
  text)


;;;; Property Drawer

(defun org-e-doxygen-property-drawer (property-drawer contents info)
  "Transcode a PROPERTY-DRAWER element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual
information."
  ;; The property drawer isn't exported but we want separating blank
  ;; lines nonetheless.
  "")


;;;; Quote Block

(defun org-e-doxygen-quote-block (quote-block contents info)
  "Transcode a QUOTE-BLOCK element from Org to Doxygen.
CONTENTS holds the contents of the block.  INFO is a plist
holding contextual information."
  content)


;;;; Quote Section

(defun org-e-doxygen-quote-section (quote-section contents info)
  "Transcode a QUOTE-SECTION element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (let ((value (org-remove-indentation
		(org-element-property :value quote-section))))
    (when value (format "\\begin{verbatim}\n%s\\end{verbatim}" value))))


;;;; Section

(defun org-e-doxygen-section (section contents info)
  "Transcode a SECTION element from Org to Doxygen.
CONTENTS holds the contents of the section.  INFO is a plist
holding contextual information."
  contents)


;;;; Radio Target

;;;; Special Block

(defun org-e-doxygen-special-block (special-block contents info)
  "Transcode a SPECIAL-BLOCK element from Org to Doxygen.
CONTENTS holds the contents of the block.  INFO is a plist
holding contextual information."
  (let ((type (downcase (org-element-property :type special-block))))
    (org-e-doxygen--wrap-label
     special-block
     (format "\\begin{%s}\n%s\\end{%s}" type contents type))))


;;;; Src Block

(defun org-e-doxygen-src-block (src-block contents info)
  "Transcode a SRC-BLOCK element from Org to Doxygen.
CONTENTS holds the contents of the item.  INFO is a plist holding
contextual information."
  (let* ((lang (org-element-property :language src-block))
	 (code (org-export-handle-code src-block info))
	 (caption (org-element-property :caption src-block))
	 (label (org-element-property :name src-block)))
    (format "@code\n%s@endcode" code)))


;;;; Statistics Cookie

(defun org-e-doxygen-statistics-cookie (statistics-cookie contents info)
  "Transcode a STATISTICS-COOKIE object from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (org-element-property :value statistics-cookie))


;;;; Subscript

(defun org-e-doxygen-subscript (subscript contents info)
  "Transcode a SUBSCRIPT object from Org to Doxygen.
CONTENTS is the contents of the object.  INFO is a plist holding
contextual information."
  (format (if (= (length contents) 1) "$_%s$" "$_{\\mathrm{%s}}$") contents))


;;;; Superscript

(defun org-e-doxygen-superscript (superscript contents info)
  "Transcode a SUPERSCRIPT object from Org to Doxygen.
CONTENTS is the contents of the object.  INFO is a plist holding
contextual information."
  (format (if (= (length contents) 1) "$^%s$" "$^{\\mathrm{%s}}$") contents))


;;;; Table

(defun org-e-doxygen-table (table contents info)
  "Transcode a TABLE element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (let ((attr (mapconcat #'identity
			 (org-element-property :attr_doxygen table)
			 " "))
	(raw-table (org-element-property :raw-table table)))
    (cond
     ;; Case 1: verbatim table.
     ((and attr (string-match "\\<verbatim\\>" attr))
      (format "@verbatim\n%s\n@endverbatim"
	      (org-export-clean-table
	       raw-table
	       (plist-get (org-export-table-format-info raw-table)
			  :special-column-p))))
     ;; Case 2: table.el table.  Convert it using appropriate tools. (TODO: doxygen)
     ((eq (org-element-property :type table) 'table.el)
      (require 'table)
      ;; Ensure "*org-export-table*" buffer is empty.
      (with-current-buffer (get-buffer-create "*org-export-table*")
	(erase-buffer))
      (let ((output (with-temp-buffer
		      (insert raw-table)
		      (goto-char 1)
		      (re-search-forward "^[ \t]*|[^|]" nil t)
		      (table-generate-source 'doxygen "*org-export-table*")
		      (with-current-buffer "*org-export-table*"
			(org-trim (buffer-string))))))
	(kill-buffer (get-buffer "*org-export-table*"))
	;; Remove left out comments.
	(while (string-match "^%.*\n" output)
	  (setq output (replace-match "" t t output)))
	;; When the "rmlines" attribute is provided, remove all hlines
	;; but the the one separating heading from the table body.
	(when (and attr (string-match "\\<rmlines\\>" attr))
	  (let ((n 0) (pos 0))
	    (while (and (< (length output) pos)
			(setq pos (string-match "^\\\\hline\n?" output pos)))
	      (incf n)
	      (unless (= n 2)
		(setq output (replace-match "" nil nil output))))))
	(if (not org-e-doxygen-tables-centered) output
	  (format "\\begin{center}\n%s\n\\end{center}" output))))
     ;; Case 3: Standard table.
     (t
      (let* ((table-info (org-export-table-format-info raw-table))
	     (columns-number (length (plist-get table-info :alignment)))
	     (longtablep (and attr (string-match "\\<longtable\\>" attr)))
	     (booktabsp
	      (and attr (string-match "\\<booktabs=\\(yes\\|t\\)\\>" attr)))
	     ;; CLEAN-TABLE is a table turned into a list, much like
	     ;; `org-table-to-lisp', with special column and
	     ;; formatting cookies removed, and cells already
	     ;; transcoded.
	     (clean-table
	      (mapcar
	       (lambda (row)
		 (if (string-match org-table-hline-regexp row) 'hline
		   (mapcar
		    (lambda (cell)
		      (org-export-secondary-string
		       (org-element-parse-secondary-string
			cell
			(cdr (assq 'table org-element-string-restrictions)))
		       'e-doxygen info))
		    (org-split-string row "[ \t]*|[ \t]*"))))
	       (org-split-string
		(org-export-clean-table
		 raw-table (plist-get table-info :special-column-p))
		"\n"))))
	;; If BOOKTABSP is non-nil, remove any rule at the beginning
	;; and the end of the table, since booktabs' special rules
	;; will be inserted instead.
	(when booktabsp
	  (when (eq (car clean-table) 'hline)
	    (setq clean-table (cdr clean-table)))
	  (when (eq (car (last clean-table)) 'hline)
	    (setq clean-table (butlast clean-table))))
	(orgtbl-to-generic clean-table
	 '(:tstart "<table>" :tend "</table>" :hline nil
		   :lstart "<tr>" :lend "</tr>" :fmt "<td>%s</td>" :hfmt "<th>%s</th>")))))))

;;;; Target

(defun org-e-doxygen-target (target text info)
  "Transcode a TARGET object from Org to Doxygen.
TEXT is the text of the target.  INFO is a plist holding
contextual information."
  (format "@anchor %s %s"
	  (org-export-solidify-link-text
	   (org-element-property :value target))
	  text))


;;;; Time-stamp

(defun org-e-doxygen-time-stamp (time-stamp contents info)
  "Transcode a TIME-STAMP object from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual
information."
  (let ((value (org-element-property :value time-stamp))
	(type (org-element-property :type time-stamp))
	(appt-type (org-element-property :appt-type time-stamp)))
    (concat (cond ((eq appt-type 'scheduled)
		   (format "\\textbf{\\textsc{%s}} " org-scheduled-string))
		  ((eq appt-type 'deadline)
		   (format "\\textbf{\\textsc{%s}} " org-deadline-string))
		  ((eq appt-type 'closed)
		   (format "\\textbf{\\textsc{%s}} " org-closed-string)))
	    (cond ((memq type '(active active-range))
		   (format org-e-doxygen-active-timestamp-format value))
		  ((memq type '(inactive inactive-range))
		   (format org-e-doxygen-inactive-timestamp-format value))
		  (t
		   (format org-e-doxygen-diary-timestamp-format value))))))


;;;; Verbatim

(defun org-e-doxygen-verbatim (verbatim contents info)
  "Transcode a VERBATIM object from Org to Doxygen.
CONTENTS is nil.  INFO is a plist used as a communication
channel."
  (format "<tt>%s</tt>" (org-element-property :value verbatim)))


;;;; Verse Block

(defun org-e-doxygen-verse-block (verse-block contents info)
  "Transcode a VERSE-BLOCK element from Org to Doxygen.
CONTENTS is nil.  INFO is a plist holding contextual information."
  (org-e-doxygen--wrap-label
   verse-block
   ;; In a verse environment, add a line break to each newline
   ;; character and change each white space at beginning of a line
   ;; into a space of 1 em.  Also change each blank line with
   ;; a vertical space of 1 em.
   (progn
     (setq contents (replace-regexp-in-string
		     "^ *\\\\\\\\$" "\\\\vspace*{1em}"
		     (replace-regexp-in-string
		      "\\(\\\\\\\\\\)?[ \t]*\n" " \\\\\\\\\n"
		      (org-remove-indentation
		       (org-export-secondary-string
			(org-element-property :value verse-block)
			'e-doxygen info)))))
     (while (string-match "^[ \t]+" contents)
       (let ((new-str (format "\\hspace*{%dem}"
			      (length (match-string 0 contents)))))
	 (setq contents (replace-match new-str nil t contents))))
     (format "\\begin{verse}\n%s\\end{verse}" contents))))


;;; Interactive functions

(defun org-e-doxygen-export-to-doxygen
  (&optional subtreep visible-only body-only ext-plist pub-dir)
  "Export current buffer to a Doxygen file.

If narrowing is active in the current buffer, only export its
narrowed part.

If a region is active, export that region.

When optional argument SUBTREEP is non-nil, export the sub-tree
at point, extracting information from the headline properties
first.

When optional argument VISIBLE-ONLY is non-nil, don't export
contents of hidden elements.

When optional argument BODY-ONLY is non-nil, only write the
content of doxygen comment block. Do not add the comment itself.

EXT-PLIST, when provided, is a property list with external
parameters overriding Org default settings, but still inferior to
file-local settings.

When optional argument PUB-DIR is set, use it as the publishing
directory.

Return output file's name."
  (interactive)
  (let ((outfile (org-export-output-file-name ".dox" subtreep pub-dir)))
    (org-export-to-file
     'e-doxygen outfile subtreep visible-only body-only ext-plist)))

(provide 'org-e-doxygen)
 ;;; org-e-doxygen.el ends here
