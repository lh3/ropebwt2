##Introduction

RopeBWT2 is an *experimental* tool for constructing the FM-index for a
collection of DNA sequences. It works by incrementally inserting multiple
sequences into an existing pseudo-BWT position by position, starting from the
end of the sequences. This algorithm can be largely considered a mixture of
[BCR][2] and [dynamic FM-index][3]. Nonetheless, ropeBWT2 is unique in that it
may *implicitly* sort the input into reverse lexicographical order (RLO) or
reverse-complement lexicographical order (RCLO) while building the index.
Suppose we have file `seq.txt` in the one-sequence-per-line format. RLO can be
achieved with Unix commands:

    rev seq.txt | sort | rev

RopeBWT2 is able to perform sorting together with the construction of the
FM-index. As such, the following command lines give the same output:

    shuf seq.txt | ropebwt2 -LRs | md5sum
	rev seq.txt | sort | rev | ropebwt2 -LR | md5sum

Here option `-s` enables the RLO mode. Similarly, the following command lines
give the same output (option `-r` enables RCLO):

    shuf seq.txt | ropebwt2 -LRr
	rev seq.txt | tr "ACGT" "TGCA" | sort | tr "ACGT" "TGCA" | rev | ropebwt2 -LR

RLO/RCLO has two benefits. Firstly, such ordering clusters sequences with
similar suffixes and helps to improve the compressibility especially for short
reads ([Cox et al, 2012][4]). Secondly, when we put both forward and reverse
complement sequences in RCLO, the resulting index has a nice feature: the
reverse complement of the *k*-th sequence in the FM-index is the *k*-th
smallest sequence. This would eliminate an array to map the rank of a sequence
to its index in the input. This array is usually not compressible unless the
input is sorted.

##Methods Overview

###Run-length encoding

###Rope on B+-tree

###Constructing the FM-index

##Comparison to Similar Algorithms

###RopeBWT2 vs. ropeBWT

RopeBWT is the predecessor of ropeBWT2. The old version implements three
separate algorithms: in-memory BCR, single-string incremental FM-index on top
of a B+ tree and single-string incremental FM-index on top of a red-black tree.
BCR is later extended to support RLO and RCLO as well.

The BCR implementation in ropeBWT is the fastest so far for short reads, faster
than ropeBWT2. This BCR implementation is still the preferred choice for
constructing the FM-index of short reads for the assembly purpose. However,
with parallelized incremental multi-string insertion, ropeBWT2 is faster than
ropeBWT for incremental index construction and works much better with long
strings in comparison to BCR. RopeBWT2 also uses more advanced run-length
encoding, which will be more space efficient for highly repetitive inputs and
might make it easier for new algorithms. It is also possible to optimize
ropeBWT2 further by caching visited nodes, though such an improvement is
technically complex.

###RopeBWT2 vs. BEETL

[BEETL][5] is the original implementation of the BCR algorithm. It uses disk to
alleviate the memory requirement for constructing large FM-index and therefore
heavily relies on fast linear disk access. BEETL supports SAP order but not RLO
or RCLO.

###RopeBWT2 vs. dynamic FM-index

RopeBWT was conceived in 2012, but the algorithm has been invented several
years earlier for multiple times. [Dynamic FM-index][3] is a notable
implementation that uses a [wavelet tree][6] for generic text and supports the
deletion operation.



[1]: https://github.com/lh3/ropebwt
[2]: http://dx.doi.org/10.1007/978-3-642-21458-5_20
[3]: http://dfmi.sourceforge.net/
[4]: https://www.ncbi.nlm.nih.gov/pubmed/22556365
[5]: https://github.com/BEETL/BEETL
[6]: https://en.wikipedia.org/wiki/Wavelet_Tree
