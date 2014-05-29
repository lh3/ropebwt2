.SUFFIXES: .gp .tex .eps .pdf .eps.gz

.eps.pdf:
		epstopdf --outfile $@ $<

.eps.gz.pdf:
		gzip -dc $< | epstopdf --filter > $@

.pdf.eps:
		pdftops -eps $< $@

all:ropebwt2.pdf

ropebwt2.pdf:ropebwt2.tex ropebwt2.bib
		pdflatex ropebwt2; bibtex ropebwt2; pdflatex ropebwt2; pdflatex ropebwt2;

clean:
		rm -fr *.toc *.aux *.bbl *.blg *.idx *.log *.out *~ varcmp.pdf varcmp.ps varcmp.dvi indel-exam.eps qst5/hist-snp.* qst5/hist-indel.* qst5/roc1-co4.* hist-snp.* hist-indel.* roc1-co4.* \
			*.eps NA12878-*.pdf CHM1-*.pdf ref.pdf ref.eps
