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

RopeBWT2 is developed for indexing hundreds of billions of symbols. It has been
carefully optimized for both speed and memory usage. On an old machine with
2.3GHz Opteron 8376 CPUs, ropeBWT2 is able to index 2.4 billion 101bp reads in
30 wall-clock hours with peak memory ~45GB.


##Methods Overview

RopeBWT2 keeps the entire BWT in six B+ trees with the *i*-th tree storing the
substring B[C(i)+1..C(i+1)], where C(i) equals the number of
symbols lexicographically smaller than *i*. In each B+ tree, an internal node
keeps the count of symbols in the subtree decending from it; an external node
keeps a BWT substring in the run-length encoding. The B+ tree achieve a similar
purpose to the [rope data structure][7], which enables efficient query and
insertion. RopeBWT2 uses this rope-like data structure to achieve incremental
construction. This is where word 'rope' in ropeBWT2 comes from.

The original BCR implementation uses static encoding to keep BWT. Although it
is possible to insert strings to an existing BWT, we have to read through the
old BWT. Reading the entire BWT may be much slower than the actual insertion.
With the rope data structure, we can insert one or a few sequences of length
*m* in O(mlogn) time without reading all the BWT. We can thus achieve
efficient incremental construction.

To achieve RLO for one-string insertion, we insert the symbol that is ahead of
a suffix at the position based on the rank of the suffix computed from backward
search. Inserting multiple strings in RLO is essentially a combination of radix
sort, BCR and single-string insertion. RopeBWT2 uses radix sort to group
symbols with an identical suffix, compute the rank of the suffix with backward
search and insert the group of symbols based on the BCR theory. For RCLO, we
find the insertion points with the complemented sequence but insert with the
original string.


###RopeBWT2 vs. ropeBWT

RopeBWT is the predecessor of ropeBWT2. The old version implements three
separate algorithms: in-memory BCR, single-string incremental FM-index on top
of a B+ tree and single-string incremental FM-index on top of a red-black tree.
BCR is later extended to support RLO and RCLO as well.

The BCR implementation in ropeBWT is the fastest so far for short reads, faster
than ropeBWT2. The legacy BCR implementation is still the preferred choice for
constructing the FM-index of short reads for the assembly purpose. However,
with parallelized incremental multi-string insertion, ropeBWT2 is faster than
ropeBWT for incremental index construction and works much better with long
strings. RopeBWT2 also uses more advanced run-length encoding, which will be
more space efficient for highly repetitive inputs and opens the possibility for
inserting a run in addition individual symbols.

###RopeBWT2 vs. BEETL

[BEETL][5] is the original implementation of the BCR algorithm. It uses disk to
alleviate the memory requirement for constructing large FM-index and therefore
heavily relies on fast linear disk access. BEETL supports the SAP order for
inserting sequences but not RLO or RCLO.

###RopeBWT2 vs. dynamic FM-index

RopeBWT was conceived in 2012, but the algorithm has been invented several
years earlier for multiple times. [Dynamic FM-index][3] is a notable
implementation that uses a [wavelet tree][6] for generic text and supports the
deletion operation. As it is not specifically designed for DNA sequences, it is
apparently ten times slower and uses more memory on the index construction.



[1]: https://github.com/lh3/ropebwt
[2]: http://dx.doi.org/10.1007/978-3-642-21458-5_20
[3]: http://dfmi.sourceforge.net/
[4]: https://www.ncbi.nlm.nih.gov/pubmed/22556365
[5]: https://github.com/BEETL/BEETL
[6]: https://en.wikipedia.org/wiki/Wavelet_Tree
[7]: https://en.wikipedia.org/wiki/Rope_%28data_structure%29
