##Introduction

RopeBWT2 is an tool for constructing the FM-index for a collection of DNA
sequences. It works by incrementally inserting one or multiple sequences into an
existing pseudo-BWT position by position, starting from the end of the
sequences. This algorithm can be largely considered a mixture of [BCR][2] and
[dynamic FM-index][3]. Nonetheless, ropeBWT2 is unique in that it may
*implicitly* sort the input into reverse lexicographical order (RLO) or
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
carefully optimized for both speed and memory usage. On a new machine with [Xeon
E5-2697v2 CPUs at 2.70GHz][cpu], ropeBWT2 is able to the index for 1.2 billion
101bp reads in five wall-clock hours with 34G peak memory.


##Examples

1. Construct the BWT for sequences in the input order:

        ropebwt2 -o out.bwt in.fa

2. Construct the BWT for sequences in RLO and output the BWT in the ropebwt2
   binary format:

        ropebwt2 -bso out.fmr in.fa

3. Construct the BWT for sequences in RLO, processing 4GB symbols at a time
   with multithreading:

        ropebwt2 -brm4g in.fa > out.fmr

   Note that for sequence reads, processing multiple sequences together is
   faster due to possible multi-threading and fewer cache misses. The peak
   memory is about *B*+*m*\*(1+48/*l*), where *B* is the size of the final BWT
   encoded in a B+-tree, *m* is the parameter value of '-m' and *l* is the
   average read length.

4. Add sequences to an existing index with the sorting order defined by the
   existing index (incremental construction):

        ropebwt2 -bi in.fmr in.fa > out.fmr


##Methods Overview

RopeBWT2 keeps the entire BWT in six B+ trees with the *i*-th tree storing the
substring B[C(i)+1..C(i+1)], where C(i) equals the number of
symbols lexicographically smaller than *i*. In each B+ tree, an internal node
keeps the count of symbols in the subtree descending from it; an external node
keeps a BWT substring in the run-length encoding. The B+ tree achieve a similar
purpose to the [rope data structure][7], which enables efficient query and
insertion. RopeBWT2 uses this rope-like data structure to achieve incremental
construction. This is where word 'rope' in ropeBWT2 comes from.

The original BCR implementation uses static encoding to keep BWT. Although it is
possible to insert strings to an existing BWT, we have to read through the old
BWT. Reading the entire BWT may be much slower than the actual insertion. With
the rope data structure, we can insert one or many sequences of length *m* in
O(mlogn) time without reading all the BWT. We can thus achieve efficient
incremental construction.

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



##Performance Evaluation

###Data sets

1. [**worm**] 66,764,080 100bp *C. eleganse* reads from [SRR065390][ce] with pairs
   containing any ambiguous bases filtered out. The total coverage is about 66X.

2. [**Venter**] 31,861,134 [Craig Venter reads][venter] totalled in 27,899,994,048bp.
   Reads containing ambiguous bases have been dropped.

3. [**NA12878**] 1,206,555,986 101bp human reads for sample NA12878, used in my fermi paper.
   Pairs containing ambiguous bases are filtered out. The data is at [1000g FTP][12878].
   Only read groups matching "20FUK" are used.

4. [**Moleculo**] 22,721,139 [Moleculo reads][mol] totalled in 91,476,572,938bp.
   This data set contains a few hundred ambiguous bases which are not filtered.

###Hardware and OS

CPU: 48 cores of [Xeon E5-2697 v2 at 2.70GHz][cpu]. GPU: one [Nvidia Tesla
K40][gpu]. RAM: 128GB. OS: Red Hat Enterprise Linux 6. CUDA: 5.5. File system:
Isilon OneFS network file system.

###Results

|Dataset|w/ rev|Algorithm        |Sorted|CPU   |Real  |RSS  |Comment|
|-------|:-----|:----------------|:-----|:-----|:-----|:----|:------|
|worm   |No    |[BEETL-BCR][bcr] |-     |2574s |2092s |1.8G |network disk|
|worm   |No    |[BEETL-BCR][bcr] |-     |2497s |965s  |1.8G |RAM disk|
|worm   |No    |[BEETL-BCRext][bcr]|-   |2839s |5900s |12.6M|network disk|
|worm   |No    |[nvSetBWT][nvb]  |-     |484s  |416s  |10.9G|mem: 2g/4g|
|worm   |No    |[nvSetBWT][nvb]  |-     |435s  |316s  |12.9G|mem: 4g/4g|
|worm   |No    |[nvSetBWT][nvb]  |-     |434s  |309s  |24.9G|mem: 16g/4g|
|worm   |No    |[nvSetBWT][nvb]  |-     |499s  |480s  |21.5G|mem: 16g/2g|
|worm   |No    |[ropebwt-BCR][rb]|No    |1070s |480s  |2.2G |-bORtf -abcr|
|worm   |No    |[ropebwt-bpr][rb]|-     |4279s |4296s |2.3G |-bOR|
|worm   |No    |[ropebwt-rbr][rb]|-     |8895s |8915s |2.3G |-bOR -arbr|
|worm   |No    |ropebwt2-single  |No    |5105s |5125s |2.5G |-bRm0|
|worm   |No    |ropebwt2    |No    |1611s |647s  |11.8G|-bRm10g|
|worm   |No    |ropebwt2    |Yes   |1268s |506s  |10.5G|-brRm10g|
|worm   |Yes   |ropebwt2    |No    |3566s |1384s |18.0G|-bm10g|
|worm   |Yes   |ropebwt2    |Yes   |3116s |1182s |15.9G|-brm10g|
|NA12878|No    |[BEETL-BCR][bcr] |-     |14.66h|11.18h|31.6G|network disk|
|NA12878|No    |[nvSetBWT][nvb]  |-     |19.33h|4.10h |63.8G|mem: 48g/4g|
|NA12878|No    |[ropebwt-BCR][rb]|No    |6.92h |3.29h |39.3G|-bORtf -abcr|
|NA12878|No    |ropebwt2    |No    |12.54h|5.06h |60.9G|-bRm10g|
|NA12878|No    |ropebwt2    |Yes   |12.94h|4.96h |34.0G|-brRm10g|
|Venter |No    |ropebwt2    |No    |3.98h |1.45h |22.8G|-bRm10g|
|Venter |No    |ropebwt2    |Yes   |3.95h |1.44h |22.2G|-brRm10g|
|Moleculo|No   |ropebwt2    |No    |19.46h|6.82h |20.0G|-bRm10g|

* For [ropebwt][rb] and ropebwt2, outputting the BWT to a plain text string
  takes significant time. We let them dump the BWT in their internal binary
  encoding. [BEETL][bcr] automatically chooses the RLE encoding in our
  evaluation. Changing the BEETL encoding may also affect its performance.

* nvSetBWT from NVBio aborted when '-gpu-mem 6144' or higher is specified. It
  seems that nvSetBWT uses more GPU memory than -gpu-mem according to the
  nvidia-smi report. nvSetBWT failed to build the index for Venter apparently
  due to insufficient RAM.

[1]: https://github.com/lh3/ropebwt
[2]: http://dx.doi.org/10.1007/978-3-642-21458-5_20
[3]: http://dfmi.sourceforge.net/
[4]: https://www.ncbi.nlm.nih.gov/pubmed/22556365
[5]: https://github.com/BEETL/BEETL
[6]: https://en.wikipedia.org/wiki/Wavelet_Tree
[7]: https://en.wikipedia.org/wiki/Rope_%28data_structure%29

[ce]: http://www.ncbi.nlm.nih.gov/sra/?term=SRR065390
[cpu]: http://ark.intel.com/products/75283/Intel-Xeon-Processor-E5-2697-v2-30M-Cache-2_70-GHz
[gpu]: http://www.nvidia.com/object/tesla-servers.html
[bcr]: https://github.com/BEETL/BEETL
[rb]: https://github.com/lh3/ropebwt
[sga]: https://github.com/jts/sga
[nvb]: https://github.com/NVlabs/nvbio
[mol]: http://bit.ly/mol12878
[12878]: ftp://ftp.ncbi.nih.gov/1000genomes/ftp/technical/working/20130103_high_cov_trio_bams/NA12878/alignment/
[venter]: http://bit.ly/levy2007
