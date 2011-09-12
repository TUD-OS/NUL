;;; org-xhtml.el --- XHTML export for Org-mode (uses org-lparse)

;; Copyright (C) 2004-2011 Free Software Foundation, Inc.

;; Author: Michal Sojka <sojka@os.inf.tu-dresden.de>, Carsten Dominik <carsten at orgmode dot org>
;; Keywords: outlines, hypermedia, calendar, wp
;; Homepage: http://orgmode.org
;; Version: 0.8

;; This file is not (yet) part of GNU Emacs.
;; However, it is distributed under the same license.

;; GNU Emacs is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.

;; GNU Emacs is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs.  If not, see <http://www.gnu.org/licenses/>.
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;;
;;; Commentary:

;;; Code:

(require 'org-exp)
(require 'org-html) 			; FIXME; remove during merge
(require 'format-spec)
(require 'org-lparse)
(eval-when-compile (require 'cl) (require 'table) (require 'browse-url))

(declare-function org-id-find-id-file "org-id" (id))
(declare-function htmlize-region "ext:htmlize" (beg end))

(defgroup org-export-doxygen nil
  "Options specific for Doxygen export of Org-mode files."
  :tag "Org Export DOXYGEN"
  :group 'org-export)

(defconst org-export-doxygen-special-string-regexps
  '(("\\\\-" . "&shy;")
    ("---\\([^-]\\)" . "&mdash;\\1")
    ("--\\([^-]\\)" . "&ndash;\\1")
    ("\\.\\.\\." . "&hellip;"))
  "Regular expressions for special string conversion.")

(defcustom org-export-doxygen-footnotes-section "@note %s
%s

"
  "Format for the footnotes section.
Should contain a two instances of %s.  The first will be replaced with the
language-specific word for \"Footnotes\", the second one will be replaced
by the footnotes themselves."
  :group 'org-export-doxygen
  :type 'string)


(defcustom org-export-doxygen-footnote-format "<sup>%s</sup>"
  "The format for the footnote reference.
%s will be replaced by the footnote reference itself."
  :group 'org-export-doxygen
  :type 'string)

(defcustom org-export-doxygen-coding-system nil
  "Coding system for DOXYGEN export, defaults to `buffer-file-coding-system'."
  :group 'org-export-doxygen
  :type 'coding-system)

(defcustom org-export-doxygen-extension "dox"
  "The extension for exported DOXYGEN files."
  :group 'org-export-doxygen
  :type 'string)

(defcustom org-export-doxygen-xml-declaration
  '(("html" . "<?xml version=\"1.0\" encoding=\"%s\"?>")
    ("php" . "<?php echo \"<?xml version=\\\"1.0\\\" encoding=\\\"%s\\\" ?>\"; ?>"))
  "The extension for exported DOXYGEN files.
%s will be replaced with the charset of the exported file.
This may be a string, or an alist with export extensions
and corresponding declarations."
  :group 'org-export-doxygen
  :type '(choice
	  (string :tag "Single declaration")
	  (repeat :tag "Dependent on extension"
		  (cons (string :tag "Extension")
			(string :tag "Declaration")))))



(defcustom org-export-doxygen-table-tag
  "<table border=\"2\" cellspacing=\"0\" cellpadding=\"6\" rules=\"groups\" frame=\"hsides\">"
  "The HTML tag that is used to start a table.
This must be a <table> tag, but you may change the options like
borders and spacing."
  :group 'org-export-doxygen
  :type 'string)

(defcustom org-export-table-header-tags '("<th scope=\"%s\"%s>" . "</th>")
  "The opening tag for table header fields.
This is customizable so that alignment options can be specified.
The first %s will be filled with the scope of the field, either row or col.
The second %s will be replaced by a style entry to align the field.
See also the variable `org-export-doxygen-table-use-header-tags-for-first-column'.
See also the variable `org-export-doxygen-table-align-individual-fields'."
  :group 'org-export-tables
  :type '(cons (string :tag "Opening tag") (string :tag "Closing tag")))

(defcustom org-export-table-data-tags '("<td%s>" . "</td>")
  "The opening tag for table data fields.
This is customizable so that alignment options can be specified.
The first %s will be filled with the scope of the field, either row or col.
The second %s will be replaced by a style entry to align the field.
See also the variable `org-export-doxygen-table-align-individual-fields'."
  :group 'org-export-tables
  :type '(cons (string :tag "Opening tag") (string :tag "Closing tag")))

(defcustom org-export-table-row-tags '("<tr>" . "</tr>")
  "The opening tag for table data fields.
This is customizable so that alignment options can be specified.
Instead of strings, these can be Lisp forms that will be evaluated
for each row in order to construct the table row tags.  During evaluation,
the variable `head' will be true when this is a header line, nil when this
is a body line.  And the variable `nline' will contain the line number,
starting from 1 in the first header line.  For example

  (setq org-export-table-row-tags
        (cons '(if head
                   \"<tr>\"
                 (if (= (mod nline 2) 1)
                     \"<tr class=\\\"tr-odd\\\">\"
                   \"<tr class=\\\"tr-even\\\">\"))
              \"</tr>\"))

will give even lines the class \"tr-even\" and odd lines the class \"tr-odd\"."
  :group 'org-export-tables
  :type '(cons
	  (choice :tag "Opening tag"
		  (string :tag "Specify")
		  (sexp))
	  (choice :tag "Closing tag"
		  (string :tag "Specify")
		  (sexp))))

(defcustom org-export-doxygen-table-use-header-tags-for-first-column nil
  "Non-nil means format column one in tables with header tags.
When nil, also column one will use data tags."
  :group 'org-export-tables
  :type 'boolean)

(defcustom org-export-doxygen-protect-char-alist
  '(("&" . "&amp;")
    ("<" . "&lt;")
    (">" . "&gt;"))
  "Alist of characters to be converted by `org-Doxygen-protect'."
  :type '(repeat (cons (string :tag "Character")
		       (string :tag "DOXYGEN equivalent"))))

(defgroup org-export-doxygenize nil
  "Options for processing examples with htmlize.el."
  :tag "Org Export Doxygenize"
  :group 'org-export-doxygen)

;;; Hooks

(defvar org-export-doxygen-after-blockquotes-hook nil
  "Hook run during DOXYGEN export, after blockquote, verse, center are done.")

(defvar org-export-doxygen-final-hook nil
  "Hook run at the end of DOXYGEN export, in the new buffer.")

(defun org-export-doxygen-preprocess-latex-fragments ()
  (when (equal org-lparse-backend 'doxygen)
    (org-export-doxygen-do-preprocess-latex-fragments)))

(defvar org-lparse-opt-plist)		    ; bound during org-do-lparse
(defun org-export-doxygen-do-preprocess-latex-fragments ()
  "Convert LaTeX fragments to images."
  (let* ((latex-frag-opt (plist-get org-lparse-opt-plist :LaTeX-fragments))
	 (latex-frag-opt-1		; massage the options
	  (cond
	   ((eq latex-frag-opt 'verbatim) 'verbatim)
	   ((eq latex-frag-opt 'dvipng  ) 'dvipng)
	   (t nil))))
    (when (and org-current-export-file latex-frag-opt)
      (org-format-latex
       (concat "ltxpng/" (file-name-sans-extension
			  (file-name-nondirectory
			   org-current-export-file)))
       org-current-export-dir nil "Creating LaTeX image %s"
       nil nil latex-frag-opt-1))))

(defun org-export-doxygen-preprocess-label-references ()
  (goto-char (point-min))
  (let (label l1)
    (while (re-search-forward "\\\\ref{\\([^{}\n]+\\)}" nil t)
      (org-if-unprotected-at (match-beginning 1)
	(setq label (match-string 1))
	(save-match-data
	  (if (string-match "\\`[a-z]\\{1,10\\}:\\(.+\\)" label)
	      (setq l1 (substring label (match-beginning 1)))
	    (setq l1 label)))
	(replace-match (format "[[#%s][%s]]" label l1) t t)))))

(defun org-export-doxygen-preprocess (parameters)
  (org-export-doxygen-preprocess-label-references))

;; Process latex fragments as part of
;; `org-export-preprocess-after-blockquote-hook'. Note that this hook
;; is the one that is closest and well before the call to
;; `org-export-attach-captions-and-attributes' in
;; `org-export-preprocess-stirng'.  The above arrangement permits
;; captions, labels and attributes to be attached to png images
;; generated out of latex equations.
(add-hook 'org-export-preprocess-after-blockquote-hook
	  'org-export-doxygen-preprocess-latex-fragments)

(defvar doxygen-table-tag nil) ; dynamically scoped into this.


;; FIXME: it already exists in org-html.el
(defconst org-doxygen-cvt-link-fn
   nil
   "Function to convert link URLs to exportable URLs.
Takes two arguments, TYPE and PATH.
Returns exportable url as (TYPE PATH), or nil to signal that it
didn't handle this case.
Intended to be locally bound around a call to `org-export-as-doxygen'." )

;; FIXME: it already exists in org-doxygen.el
(defun org-doxygen-cvt-org-as-doxygen (opt-plist type path)
   "Convert an org filename to an equivalent doxygen filename.
If TYPE is not file, just return `nil'.
See variable `org-export-xdoxygen-link-org-files-as-doxygen'"
   (save-match-data
      (and
	 (string= type "file")
	 (string-match "\\.org$" path)
	 (progn
	    (list
	       "file"
	       (concat
		  (substring path 0 (match-beginning 0))
		  ".dox"))))))

;;; org-doxygen-format-org-link
(defun org-doxygen-format-org-link (opt-plist type-1 path fragment desc attr
					   descp)
  "Make an DOXYGEN link.
OPT-PLIST is an options list.
TYPE is the device-type of the link (THIS://foo.html)
PATH is the path of the link (http://THIS#locationx)
FRAGMENT is the fragment part of the link, if any (foo.html#THIS)
DESC is the link description, if any.
ATTR is a string of other attributes of the a element.
MAY-INLINE-P allows inlining it as an image."
  (declare (special org-lparse-par-open))
  (when (string= type-1 "coderef")
    (setq attr
	  (format "class=\"coderef\" onmouseover=\"CodeHighlightOn(this, '%s');\" onmouseout=\"CodeHighlightOff(this, '%s');\""
		  fragment fragment)))
  (save-match-data
    (let* ((may-inline-p
	    (and (member type-1 '("http" "https" "file"))
		 (org-lparse-should-inline-p path descp)
		 (not fragment)))
	   (type (if (equal type-1 "id") "file" type-1))
	   (filename path)
	   ;;First pass.  Just sanity stuff.
	   (components-1
	    (cond
	     ((string= type "file")
	      (list
	       type
	       ;;Substitute just if original path was absolute.
	       ;;(Otherwise path must remain relative)
	       (if (file-name-absolute-p path)
		   (concat "file://" (expand-file-name path))
		 path)))
	     ((string= type "")
	      (list nil path))
	     (t (list type path))))

	   ;;Second pass.  Components converted so they can refer
	   ;;to a remote site.
	   (components-2
	    (or
	     (and org-doxygen-cvt-link-fn
		  (apply org-doxygen-cvt-link-fn
			 opt-plist components-1))
	     (apply #'org-doxygen-cvt-org-as-doxygen
		    opt-plist components-1)
	     components-1))
	   (type    (first  components-2))
	   (thefile (second components-2)))


      ;;Third pass.  Build final link except for leading type
      ;;spec.
      (cond
       ((or
	 (not type)
	 (string= type "http")
	 (string= type "https")
	 (string= type "file")
	 (string= type "coderef"))
	(if fragment
	    (setq thefile (concat thefile "#" fragment))))

       (t))

      ;;Final URL-build, for all types.
      (setq thefile
	    (let
		((str (org-xml-format-href thefile)))
	      (if (and type (not (or (string= "file" type)
				     (string= "coderef" type))))
		  (concat type ":" str)
		str)))

      (if may-inline-p
	  (org-doxygen-format-image thefile)
	(org-lparse-format
	 'LINK (org-xml-format-desc desc) thefile attr)))))

(defun org-doxygen-format-inline-image (desc)
  ;; FIXME: alt text missing here?
  (org-doxygen-format-tags "@image html %s" "" desc))


;; FIXME: the org-lparse defvar belongs to org-lparse.el
(defvar org-lparse-link-description-is-image)

(defun org-doxygen-format-image (src)
  "Create image tag with source and attributes."
  (save-match-data
    (let* ((caption (org-find-text-property-in-string 'org-caption src))
	   (attr (org-find-text-property-in-string 'org-attributes src))
	   (label (org-find-text-property-in-string 'org-label src))
	   (caption (and caption (org-xml-encode-org-text caption)))
	   (extra (concat
		   (and label
			(format "id=\"%s\" " (org-solidify-link-text label)))
		   "class=\"figure\"")))
      (format "@image html %s%s" src (if caption (format " \"%s\"" caption) "")))))


(defun org-export-doxygen-get-bibliography ()
  "Find bibliography, cut it out and return it."
  (catch 'exit
    (let (beg end (cnt 1) bib)
      (save-excursion
	(goto-char (point-min))
	(when (re-search-forward "^[ \t]*<div \\(id\\|class\\)=\"bibliography\"" nil t)
	  (setq beg (match-beginning 0))
	  (while (re-search-forward "</?div\\>" nil t)
	    (setq cnt (+ cnt (if (string= (match-string 0) "<div") +1 -1)))
	    (when (= cnt 0)
	      (and (looking-at ">") (forward-char 1))
	      (setq bib (buffer-substring beg (point)))
	      (delete-region beg (point))
	    (throw 'exit bib))))
	nil))))

(defun org-doxygen-format-table (lines olines)
  (let ((org-doxygen-format-table-no-css nil))
    (org-lparse-format-table lines olines)))

;; Following variable is defined for native tables i.e., when
;; `org-lparse-table-is-styled' evals to t.
(defvar org-doxygen-format-table-no-css)
(defvar org-table-number-regexp) ; defined in org-table.el

;; FIXME: This function is called from other modules. Use doxygen suffix
;; to avoid conflict
(defun org-format-table-doxygen (lines olines &optional no-css)
  "Find out which DOXYGEN converter to use and return the DOXYGEN code.
NO-CSS is passed to the exporter."
  (let* ((org-lparse-backend 'doxygen)
	 (org-lparse-entity-control-callbacks-alist
	  (org-lparse-get 'ENTITY-CONTROL))
	 (org-lparse-entity-format-callbacks-alist
	  (org-lparse-get 'ENTITY-FORMAT))
	 (org-doxygen-format-table-no-css no-css))
    (org-lparse-format-table lines olines)))

;; FIXME: This function is called from other modules. Use doxygen suffix
;; to avoid conflict
(defun org-format-org-table-doxygen (lines &optional splice no-css)
  ;; This routine might get called outside of org-export-as-doxygen. For
  ;; example, this could happen as part of org-table-export or as part
  ;; of org-export-as-docbook. Explicitly bind the parser callback to
  ;; the doxygen ones for the duration of the call.
  (let* ((org-lparse-backend 'doxygen)
	 (org-lparse-entity-control-callbacks-alist
	  (org-lparse-get 'ENTITY-CONTROL))
	 (org-lparse-entity-format-callbacks-alist
	  (org-lparse-get 'ENTITY-FORMAT))
	 (org-doxygen-format-table-no-css no-css))
    (org-lparse-format-org-table lines splice)))


;; FIXME: it already exists in org-doxygen.el
(defun org-export-splice-attributes (tag attributes)
  "Read attributes in string ATTRIBUTES, add and replace in DOXYGEN tag TAG."
  (if (not attributes)
      tag
    (let (oldatt newatt)
      (setq oldatt (org-extract-attributes-from-string tag)
	    tag (pop oldatt)
	    newatt (cdr (org-extract-attributes-from-string attributes)))
      (while newatt
	(setq oldatt (plist-put oldatt (pop newatt) (pop newatt))))
      (if (string-match ">" tag)
	  (setq tag
		(replace-match (concat (org-attributes-to-string oldatt) ">")
			       t t tag)))
      tag)))

;; FIXME: This function is called from other modules. Use doxygen suffix
;; to avoid conflict
(defun org-format-table-table-doxygen (lines)
  (let* ((org-lparse-get 'doxygen)
	 (org-lparse-entity-control-callbacks-alist
	  (org-lparse-get 'ENTITY-CONTROL))
	 (org-lparse-entity-format-callbacks-alist
	  (org-lparse-get 'ENTITY-FORMAT)))
    (org-lparse-format-table-table lines)))


;; FIXME: it already exists in org-html.el
(defun org-export-splice-style (style extra)
  "Splice EXTRA into STYLE, just before \"</style>\"."
  (if (and (stringp extra)
	   (string-match "\\S-" extra)
	   (string-match "</style>" style))
      (concat (substring style 0 (match-beginning 0))
	      "\n" extra "\n"
	      (substring style (match-beginning 0)))
    style))


(defvar body-only) ; dynamically scoped into this.

;; Following variable is let bound when `org-do-lparse' is in
;; progress. See org-lparse.el.

;; FIXME: the org-lparse defvar belongs to org-lparse.el
(defvar org-lparse-toc)
(defvar org-lparse-footnote-definitions)
(defvar org-lparse-dyn-first-heading-pos)

(defun org-doxygen-end-export ()
  ;; insert the table of contents
  (when (and org-export-with-toc (not body-only) org-lparse-toc)
    (org-doxygen-insert-toc org-lparse-toc))

  ;; remove empty paragraphs
  (goto-char (point-min))
  (while (re-search-forward "<p>[ \r\n\t]*</p>" nil t)
    (replace-match ""))

  ;; Convert whitespace place holders
  (goto-char (point-min))
  (let (beg end n)
    (while (setq beg (next-single-property-change (point) 'org-whitespace))
      (setq n (get-text-property beg 'org-whitespace)
	    end (next-single-property-change beg 'org-whitespace))
      (goto-char beg)
      (delete-region beg end)
      (insert (format "<span style=\"visibility:hidden;\">%s</span>"
		      (make-string n ?x)))))

  ;; Remove empty lines at the beginning of the file.
  (goto-char (point-min))
  (when (looking-at "\\s-+\n") (replace-match ""))

  ;; Remove display properties
  (remove-text-properties (point-min) (point-max) '(display t))

  ;; Run the hook
  (run-hooks 'org-export-doxygen-final-hook))

(defun org-doxygen-format-toc-entry (snumber todo headline tags href)
  (setq headline (concat
		  (and org-export-with-section-numbers
		       (concat snumber " "))
		  headline
		  (and tags
		    (concat
		     (org-lparse-format 'SPACES 3)
		     (org-lparse-format 'FONTIFY tags "tag")))))
  (when todo
    (setq headline (org-lparse-format 'FONTIFY headline "todo")))
  (org-lparse-format 'LINK headline (concat  "#" href)))

(defun org-doxygen-format-toc-item (toc-entry level org-last-level)
  (when (> level org-last-level)
    (let ((cnt (- level org-last-level)))
      (while (>= (setq cnt (1- cnt)) 0)
	(org-lparse-begin-list 'unordered)
	(org-lparse-begin-list-item 'unordered))))
  (when (< level org-last-level)
    (let ((cnt (- org-last-level level)))
      (while (>= (setq cnt (1- cnt)) 0)
	(org-lparse-end-list-item-1)
	(org-lparse-end-list 'unordered))))

  (org-lparse-end-list-item-1)
  (org-lparse-begin-list-item 'unordered)
  (insert toc-entry))

(defun org-doxygen-begin-toc (lang-specific-heading)
  (org-lparse-insert-tag "<div id=\"table-of-contents\">")
  (insert
   (org-lparse-format 'HEADING lang-specific-heading
		     (or (org-lparse-get 'TOPLEVEL-HLEVEL) 1)))
  (org-lparse-insert-tag "<div id=\"text-table-of-contents\">")
  (org-lparse-begin-list 'unordered)
  (org-lparse-begin-list-item 'unordered))

(defun org-doxygen-end-toc ()
  (while (> org-last-level (1- org-min-level))
    (setq org-last-level (1- org-last-level))
    (org-lparse-end-list-item-1)
    (org-lparse-end-list 'unordered))
  (org-lparse-insert-tag "</div>")
  (org-lparse-insert-tag "</div>")

  ;; cleanup empty list items in toc
  (while (re-search-backward "<li>[ \r\n\t]*</li>\n?" (point-min) t)
    (replace-match "")))

;;;###autoload
(defun org-export-as-doxygen-and-open (arg)
  "Export the outline as DOXYGEN and immediately open it with a browser.
If there is an active region, export only the region.
The prefix ARG specifies how many levels of the outline should become
headlines.  The default is 3.  Lower levels will become bulleted lists."
  (interactive "P")
  (org-lparse-and-open "doxygen" "doxygen" arg))

;;;###autoload
(defun org-export-as-doxygen-batch ()
  "Call the function `org-lparse-batch'.
This function can be used in batch processing as:
emacs   --batch
        --load=$HOME/lib/emacs/org.el
        --eval \"(setq org-export-headline-levels 2)\"
        --visit=MyFile --funcall org-export-as-doxygen-batch"
  (org-lparse-batch "doxygen"))

;;;###autoload
(defun org-export-as-doxygen-to-buffer (arg)
  "Call `org-lparse-to-buffer` with output to a temporary buffer.
No file is created.  The prefix ARG is passed through to `org-lparse-to-buffer'."
  (interactive "P")
  (org-lparse-to-buffer "doxygen" arg))

;;;###autoload
(defun org-replace-region-by-doxygen (beg end)
  "Assume the current region has org-mode syntax, and convert it to DOXYGEN.
This can be used in any buffer.  For example, you could write an
itemized list in org-mode syntax in an DOXYGEN buffer and then use this
command to convert it."
  (interactive "r")
  (org-replace-region-by "doxygen" beg end))

;;;###autoload
(defun org-export-region-as-doxygen (beg end &optional body-only buffer)
  "Convert region from BEG to END in `org-mode' buffer to DOXYGEN.
If prefix arg BODY-ONLY is set, omit file header, footer, and table of
contents, and only produce the region of converted text, useful for
cut-and-paste operations.
If BUFFER is a buffer or a string, use/create that buffer as a target
of the converted DOXYGEN.  If BUFFER is the symbol `string', return the
produced DOXYGEN as a string and leave not buffer behind.  For example,
a Lisp program could call this function in the following way:

  (setq doxygen (org-export-region-as-doxygen beg end t 'string))

When called interactively, the output buffer is selected, and shown
in a window.  A non-interactive call will only return the buffer."
  (interactive "r\nP")
  (org-lparse-region "doxygen" beg end body-only buffer))

;;; org-export-as-doxygen
;;;###autoload
(defun org-export-as-doxygen (arg &optional hidden ext-plist
			       to-buffer body-only pub-dir)
  "Export the outline as a pretty DOXYGEN file.
Use `org-lparse' internally to perform the actual export. This
routine merely binds the TARGET-BACKEND and NATIVE-BACKEND args
of `org-lparse' to \"doxygen\"."
  (interactive "P")
  (org-lparse "doxygen" "doxygen" arg hidden ext-plist to-buffer body-only pub-dir))

(defvar org-doxygen-entity-control-callbacks-alist
  `((EXPORT
     . (org-doxygen-begin-export org-doxygen-end-export))
    (DOCUMENT-CONTENT
     . (org-doxygen-begin-document-content org-doxygen-end-document-content))
    (DOCUMENT-BODY
     . (org-doxygen-begin-document-body org-doxygen-end-document-body))
    (TOC
     . (org-doxygen-begin-toc org-doxygen-end-toc))
    (ENVIRONMENT
     . (org-doxygen-begin-environment org-doxygen-end-environment))
    (FOOTNOTE-DEFINITION
     . (org-doxygen-begin-footnote-definition org-doxygen-end-footnote-definition))
    (TABLE
     . (org-doxygen-begin-table org-doxygen-end-table))
    (TABLE-ROWGROUP
     . (org-doxygen-begin-table-rowgroup org-doxygen-end-table-rowgroup))
    (LIST
     . (org-doxygen-begin-list org-doxygen-end-list))
    (LIST-ITEM
     . (org-doxygen-begin-list-item org-doxygen-end-list-item))
    (OUTLINE
     . (org-doxygen-begin-outline org-doxygen-end-outline))
    (OUTLINE-TEXT
     . (org-doxygen-begin-outline-text org-doxygen-end-outline-text))
    (PARAGRAPH
     . (org-doxygen-begin-paragraph org-doxygen-end-paragraph)))
  "Alist of control callbacks registered with the exporter.
Each element is of the form (ENTITY . (BEGIN-ENTITY-FUNCTION
END-ENTITY-FUNCTION)).  ENTITY is one of PARAGRAPH, LIST etc as
seen above.  BEGIN-ENTITY-FUNCTION and END-ENTITY-FUNCTION are
functions that get called when the exporter needs to begin or end
an entity in the currently exported file.  The signatures of
these callbacks are specific to the ENTITY being emitted.  These
callbacks always get called with exported file as the current
buffer and need to insert the appropriate tags into the current
buffer.  For example, `org-doxygen-begin-paragraph' inserts <p> and
`org-doxygen-end-paragraph' inserts </p> in to the current buffer.
These callbacks are invoked via `org-lparse-begin' and
`org-lparse-end'.")

(defvar org-doxygen-entity-format-callbacks-alist
  `((EXTRA-TARGETS . org-lparse-format-extra-targets)
    (ORG-TAGS . org-lparse-format-org-tags)
    (SECTION-NUMBER . org-lparse-format-section-number)
    (HEADLINE . org-doxygen-format-headline)
    (TOC-ENTRY . org-doxygen-format-toc-entry)
    (TOC-ITEM . org-doxygen-format-toc-item)
    (TAGS . org-doxygen-format-tags)
    (SPACES . org-doxygen-format-spaces)
    (TABS . org-doxygen-format-tabs)
    (LINE-BREAK . org-doxygen-format-line-break)
    (FONTIFY . org-doxygen-format-fontify)
    (TODO . org-lparse-format-todo)
    (ORG-LINK . org-doxygen-format-org-link)
    (LINK . org-doxygen-format-link)
    (INLINE-IMAGE . org-doxygen-format-inline-image)
    (HEADING . org-doxygen-format-heading)
    (ANCHOR . org-doxygen-format-anchor)
    (TABLE . org-doxygen-format-table)
    (TABLE-ROW . org-doxygen-format-table-row)
    (TABLE-CELL . org-doxygen-format-table-cell)
    (FOOTNOTES-SECTION . org-doxygen-format-footnotes-section)
    (FOOTNOTE-REFERENCE . org-doxygen-format-footnote-reference)
    (HORIZONTAL-LINE . org-doxygen-format-horizontal-line)
    (LINE . org-doxygen-format-line)
    (COMMENT . org-doxygen-format-comment)
    (ORG-ENTITY . org-doxygen-format-org-entity))
  "Alist of format callbacks registered with the exporter.
Each element is of the form (ENTITY . ENTITY-FORMAT-FUNCTION).
ENTITY is one of LINE, HEADING, COMMENT, LINK, TABLE-ROW etc as
seen above.  ENTITY-FORMAT-FUNCTION is a functions that gets
called when the exporter needs to format a string in `org-mode'
buffer in a backend specific way.  The signatures of the
formatting callback is specific to the ENTITY being passed in.
These callbacks always need to encode the incoming entity in
backend specific way and return the same.  These callbacks do not
make any modifications to the exporter file.  For example,
`org-doxygen-format-table-row' encloses incoming entity in <tr>
</tr> tags and returns it.  See also `org-lparse-format'.")

;; register the doxygen exporter with org-lparse library
(org-lparse-register-backend 'doxygen)

(defun org-doxygen-unload-function ()
  (org-lparse-unregister-backend 'doxygen)
  (remove-hook 'org-export-preprocess-after-blockquote-hook
	       'org-export-doxygen-preprocess-latex-fragments)
  nil)

(defun org-doxygen-begin-document-body (opt-plist)
  (insert "@mainpage\n"))

(defun org-doxygen-end-document-body (opt-plist))

(defun org-doxygen-begin-document-content (opt-plist)
  (insert (format
	   "/* WARNING: This file was automatically generated from %s. */\n/**\n"
	   (plist-get opt-plist :macro-input-file))))

(defun org-doxygen-end-document-content ()
  (insert "\n */\n"))

(defvar org-doxygen-page-stack nil)
(make-variable-buffer-local 'org-doxygen-page-stack)
 
(defun org-doxygen-begin-outline (level1 snumber title tags
					 target extra-targets extra-class)
  (let* ((class (format "outline-%d" level1))
	 (class (if extra-class (concat  class " " extra-class) class))
	 (id (format "outline-container-%s"
		     (org-lparse-suffix-from-snumber snumber)))
	 (extra (concat (when id (format " id=\"%s\"" id))
			(when class (format " class=\"%s\"" class))))
	 (taglist (org-split-string (or tags "") ":"))
	 (pagename (format "sec-%s" (org-lparse-suffix-from-snumber snumber))))
    (if (member "page" taglist)
	(progn
	  (insert (format "\n@subpage %s\n\n" pagename))
	  (insert (format "@page %s %s %s\n\n" pagename snumber title))
	  (push (cons 'page pagename) org-doxygen-page-stack))
      (progn
	(insert
	 (org-lparse-format 'HEADING
			    (org-lparse-format
			     'HEADLINE title extra-targets tags snumber level1)
			    level1 target))
	(push (cons 'sec snumber) org-doxygen-page-stack)))))

(defun org-doxygen-end-outline ()
  (let* ((top (pop org-doxygen-page-stack))
	 (type (car top))
	 (what (cdr top))
	 (i org-doxygen-page-stack))
    (when (eq type 'page)
      (let ((pagename (cdr (assoc 'page org-doxygen-page-stack))))
	(insert
	 (cond ((not pagename) "\n@mainpage\n")
	       (t (format "\n@page %s\n" pagename))))))))

(defun org-doxygen-begin-outline-text (level1 snumber extra-class))

(defun org-doxygen-end-outline-text ())

(defun org-doxygen-begin-paragraph (&optional style))

(defun org-doxygen-end-paragraph ())

(defun org-doxygen-format-environment (style beg-end)
  (assert (memq style '(blockquote center verse fixedwidth quote native)) t)
  (case style
    (blockquote
     (case beg-end
       (BEGIN
	(org-lparse-end-paragraph)
	(insert "<blockquote>\n")
	(org-lparse-begin-paragraph))
       (END
	(org-lparse-end-paragraph)
	(insert "\n</blockquote>\n")
	(org-lparse-begin-paragraph))))
    (verse
     (case beg-end
       (BEGIN
	(org-lparse-end-paragraph)
	(insert "\n<p class=\"verse\">\n")
	(setq org-lparse-par-open t))
       (END
	(insert "</p>\n")
	(setq org-lparse-par-open nil)
	(org-lparse-begin-paragraph))))
    (center
     (case beg-end
       (BEGIN
	(org-lparse-end-paragraph)
	(insert "\n<div style=\"text-align: center\">")
	(org-lparse-begin-paragraph))
       (END
	(org-lparse-end-paragraph)
	(insert "\n</div>")
	(org-lparse-begin-paragraph))))
    (fixedwidth
     (case beg-end
       (BEGIN
	(org-lparse-end-paragraph)
	(insert "<pre class=\"example\">\n"))
       (END
	(insert "</pre>\n")
	(org-lparse-begin-paragraph))))
    (quote
     (case beg-end
       (BEGIN
	(org-lparse-end-paragraph)
	(insert "<pre>"))
       (END
	(insert "</pre>\n")
	(org-lparse-begin-paragraph))))
    (native
     (case beg-end
       (BEGIN (org-lparse-end-paragraph))
       (END (org-lparse-begin-paragraph))))
    (t (error "Unknown environment %s" style))))

(defun org-doxygen-begin-environment (style)
  (org-doxygen-format-environment style 'BEGIN))

(defun org-doxygen-end-environment (style)
  (org-doxygen-format-environment style 'END))

(defun org-doxygen-begin-list (ltype)
  (setq ltype (or (org-lparse-html-list-type-to-canonical-list-type ltype)
		  ltype))
  (case ltype
    (ordered (let* ((arg1 nil)
		    (extra (if arg1 (format " start=\"%d\"" arg1) "")))
	       (org-lparse-insert-tag "<ol%s>" extra)))
    (unordered (org-lparse-insert-tag "<ul>"))
    (description (org-lparse-insert-tag "<dl>"))
    (t (error "Unknown list type: %s"  ltype))))

(defun org-doxygen-end-list (ltype)
  (setq ltype (or (org-lparse-html-list-type-to-canonical-list-type ltype)
		  ltype))

  (org-lparse-insert-tag
     (case ltype
       (ordered "</ol>")
       (unordered "</ul>")
       (description "</dl>")
       (t (error "Unknown list type: %s" ltype)))))

(defun org-doxygen-begin-list-item (ltype &optional arg headline)
  (setq ltype (or (org-lparse-html-list-type-to-canonical-list-type ltype)
		  ltype))
  (case ltype
    (ordered
     (assert (not headline) t)
     (let* ((counter arg)
	   (extra (if counter (format " value=\"%s\"" counter) "")))
       (org-lparse-insert-tag "<li%s>" extra)))
    (unordered
     (let* ((id arg)
	   (extra (if id (format " id=\"%s\"" id) "")))
       (org-lparse-insert-tag "<li%s>" extra)
       (when headline
	 (insert headline (org-lparse-format 'LINE-BREAK) "\n"))))
    (description
     (assert (not headline) t)
     (let* ((desc-tag (or arg "(no term)")))
       (insert
	(org-doxygen-format-tags '("<dt>" . "</dt>") desc-tag))
       (org-lparse-insert-tag "<dd>")))
    (t (error "Unknown list type"))))

(defun org-doxygen-end-list-item (ltype)
  (setq ltype (or (org-lparse-html-list-type-to-canonical-list-type ltype)
		  ltype))
  (case ltype
    (ordered (org-lparse-insert-tag "</li>"))
    (unordered (org-lparse-insert-tag "</li>"))
    (description (org-lparse-insert-tag "</dd>"))
    (t (error "Unknown list type"))))

;; Following variables are let bound when table emission is in
;; progress. See org-lparse.el.

;; FIXME: the org-lparse defvar belongs to org-lparse.el
(defvar org-lparse-table-begin-marker)
(defvar org-lparse-table-ncols)
(defvar org-lparse-table-rowgrp-open)
(defvar org-lparse-table-rownum)
(defvar org-lparse-table-cur-rowgrp-is-hdr)
(defvar org-lparse-table-rowgrp-info)
(defvar org-lparse-table-colalign-vector)
(defvar org-lparse-table-num-numeric-items-per-column)

(defun org-doxygen-begin-table-rowgroup (&optional is-header-row))

(defun org-doxygen-end-table-rowgroup ())

(defun org-doxygen-begin-table (caption label attributes)
  (let ((html-table-tag
	 (org-export-splice-attributes html-table-tag attributes)))
    (when label
      (setq html-table-tag
	    (org-export-splice-attributes
	     html-table-tag
	     (format "id=\"%s\"" (org-solidify-link-text label)))))
    (org-lparse-insert-tag html-table-tag))

  ;; Since the output of HTML table formatter can also be used in
  ;; DocBook document, we want to always include the caption to make
  ;; DocBook XML file valid.
  (insert (format "<caption>%s</caption>" (or caption "")) "\n"))

(defun org-doxygen-end-table ()
  (org-lparse-insert-tag "</table>\n"))

(defun org-doxygen-format-table-row (row)
  (org-doxygen-format-tags
   (cons (eval (car org-export-table-row-tags))
	 (eval (cdr org-export-table-row-tags))) row))

(defun org-doxygen-format-table-cell (text r c)
  (let ((cell-style-cookie ""))
    (cond
     (org-lparse-table-cur-rowgrp-is-hdr
      (org-doxygen-format-tags
       org-export-table-header-tags text  "col" cell-style-cookie))
     ((and (= c 0) org-export-doxygen-table-use-header-tags-for-first-column)
      (org-doxygen-format-tags
       org-export-table-header-tags text "row" cell-style-cookie))
     (t
      (org-doxygen-format-tags
       org-export-table-data-tags text cell-style-cookie)))))

(defun org-doxygen-begin-footnote-definition (n)
  (org-lparse-begin-paragraph 'footnote)
  (insert
   (format
    (format org-export-doxygen-footnote-format
	    "<a class=\"footnum\" name=\"fn.%s\" href=\"#fnr.%s\">%s</a>")
    n n n)))

(defun org-doxygen-end-footnote-definition (n)
  (org-lparse-end-paragraph))

(defun org-doxygen-format-spaces (n)
  (let ((space (or (and org-lparse-encode-pending "\\nbsp") "&nbsp;")) out)
    (while (> n 0)
      (setq out (concat out space))
      (setq n (1- n))) out))

(defun org-doxygen-format-tabs (&optional n)
  (ignore))

(defun org-doxygen-format-line-break ()
  (org-doxygen-format-tags "<br/>" ""))

(defun org-doxygen-format-horizontal-line ()
  (concat  "\n" "<hr/>" "\n"))

(defun org-doxygen-format-line (line)
  (case org-lparse-dyn-current-environment
    ((quote fixedwidth) (concat (org-xml-encode-plain-text line) "\n"))
    (t (concat line "\n"))))

(defun org-doxygen-format-comment (fmt &rest args)
  (let ((comment (apply 'format fmt args)))
    (format "\n<!-- %s  -->\n" comment)))

(defun org-doxygen-format-fontify (text style &optional id)
  (let (class extra how)
    (cond
     ((eq style 'underline)
      (setq extra " style=\"text-decoration:underline;\"" ))
     ((setq how (cdr (assoc style
			    '((bold . ("<b>" . "</b>"))
			      (emphasis . ("<i>" . "</i>"))
			      (code . ("<code>" . "</code>"))
			      (verbatim . ("<code>" . "</code>"))
			      (strike . ("<del>" . "</del>"))
			      (subscript . ("<sub>" . "</sub>"))
			      (superscript . ("<sup>" . "</sup>")))))))
     ((listp style)
      (setq class (mapconcat 'identity style " ")))
     ((stringp style)
      (setq class style))
     (t (error "Unknown style %S" style)))

    (setq extra (concat (when class (format " class=\"%s\"" class))
			(when id (format " id=\"%s\""  id))
			extra))
    (if how
	(org-doxygen-format-tags how text)
      text)))

(defun org-doxygen-format-link (text href &optional extra)
  (let ((extra (concat (format " href=\"%s\"" href)
		       (and extra (concat  " " extra)))))
    (org-doxygen-format-tags '("<a%s>" . "</a>") text extra)))

(defun org-doxygen-format-heading (text level &optional id)
  (let ((levelrel 0)
	(i org-doxygen-page-stack))
    (while (and i (not (eq (princ (caar i)) 'page)))
      (setq levelrel (1+ levelrel))
      (setq i (cdr i)))
    (let ((sub (nth (1- levelrel) '("" "sub" "subsub"))))
      (when (not sub)
	(error "Too deep heading - use :page: tag"))
      (format "@%ssection %s %s\n\n" sub id text))))

(defun org-doxygen-format-headline (title extra-targets tags
					  &optional snumber level)
  ;; FIXME I do not like extra-targets here - I need it in org-doxygen-format-heading
  (concat
   ;(org-lparse-format 'EXTRA-TARGETS extra-targets)
   (concat snumber " ")
   title
   (and tags (concat (org-lparse-format 'SPACES 3)
		     (org-lparse-format 'ORG-TAGS tags)))))

(defun org-doxygen-format-anchor (text name &optional class)
  (let* ((id name)
	 (extra (concat
		 (when name (format " name=\"%s\""  name))
		 (when id (format " id=\"%s\""  id))
		 (when class (format " class=\"%s\""  class)))))
    (org-doxygen-format-tags '("<a%s>" . "</a>") text extra)))

(defun org-doxygen-format-footnote-reference (n def refcnt)
  (let ((extra (if (= refcnt 1) "" (format ".%d"  refcnt))))
    (format org-export-doxygen-footnote-format
	    (format
	     "<a class=\"footref\" name=\"fnr.%s%s\" href=\"#fn.%s\">%s</a>"
	     n extra n n))))

(defun org-doxygen-format-footnotes-section (section-name definitions)
  (if (not definitions) ""
    (format org-export-doxygen-footnotes-section section-name definitions)))

(defun org-doxygen-format-org-entity (wd)
  (org-entity-get-representation wd 'html))

(defun org-doxygen-format-tags (tag text &rest args)
  (let ((prefix (when org-lparse-encode-pending "@"))
	(suffix (when org-lparse-encode-pending "@")))
    (apply 'org-lparse-format-tags tag text prefix suffix args)))

(defun org-doxygen-get (what &optional opt-plist)
  (case what
    (BACKEND 'doxygen)
    (INIT-METHOD nil)
    (SAVE-METHOD nil)
    (CLEANUP-METHOD nil)
    ;; (OTHER-BACKENDS
    ;;  ;; There is a provision to register a per-backend converter and
    ;;  ;; output formats. Refer `org-lparse-get-converter' and
    ;;  ;; `org-lparse-get-other-backends'.

    ;;  ;; The default behaviour is to use `org-lparse-convert-process'
    ;;  ;; and `org-lparse-convert-capabilities'.
    ;;  )
    ;; (CONVERT-METHOD
    ;;  ;; See note above
    ;;  )
    (EXPORT-DIR "")
    (FILE-NAME-EXTENSION "dox")
    (EXPORT-BUFFER-NAME "*Org Doxygen Export*")
    (ENTITY-CONTROL org-doxygen-entity-control-callbacks-alist)
    (ENTITY-FORMAT org-doxygen-entity-format-callbacks-alist)
    (TOPLEVEL-HLEVEL 1)
    (SPECIAL-STRING-REGEXPS org-export-doxygen-special-string-regexps)
    (CODING-SYSTEM-FOR-WRITE org-export-doxygen-coding-system)
    (CODING-SYSTEM-FOR-SAVE org-export-doxygen-coding-system)
    (INLINE-IMAGES t)
    (INLINE-IMAGE-EXTENSIONS '("png" "jpeg" "jpg" "gif" "svg"))
    (PLAIN-TEXT-MAP org-export-doxygen-protect-char-alist)
    (TABLE-FIRST-COLUMN-AS-LABELS org-export-doxygen-table-use-header-tags-for-first-column)
    (TODO-KWD-CLASS-PREFIX "")
    (TAG-CLASS-PREFIX "")
    (FOOTNOTE-SEPARATOR "")
    (t (error "Unknown property: %s"  what))))

(defun org-doxygen-get-coding-system-for-write ()
  (or org-export-doxygen-coding-system
      (and (boundp 'buffer-file-coding-system) buffer-file-coding-system)))

(defun org-doxygen-get-coding-system-for-save ()
  (or org-export-doxygen-coding-system
      (and (boundp 'buffer-file-coding-system) buffer-file-coding-system)))

(defun org-doxygen-insert-toc (toc))
;;   ;; locate where toc needs to be inserted
;;   (goto-char (point-min))
;;   (cond
;;    ((or (re-search-forward "<p>\\s-*\\[TABLE-OF-CONTENTS\\]\\s-*</p>" nil t)
;; 	(re-search-forward "\\[TABLE-OF-CONTENTS\\]" nil t))
;;     (goto-char (match-beginning 0))
;;     (replace-match "")
;;     (insert toc))
;;    (org-lparse-dyn-first-heading-pos
;;     (goto-char org-lparse-dyn-first-heading-pos)
;;     (when (looking-at "\\s-*</p>")
;;       (goto-char (match-end 0))
;;       (insert "\n"))
;;     (insert toc))
;;    (t (ignore))))

(defun org-doxygen-format-source-code-or-example
  (lines lang caption textareap cols rows num cont rpllbl fmt)
  (concat "@code\n" lines "\n@endcode\n"))

(provide 'org-doxygen)

;;; org-doxygen.el ends here
