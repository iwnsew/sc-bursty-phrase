# Greedy Set Cover Algorithm for Bursty Phrase Extraction

This tool implements the method to enumerate bursty phrases in a target document set in reference with a reference document set.

Phrases that frequently occur in the target document set but not frequently in the reference document set can be bursty phrases.

The degree of the burst is the standard score (z-score) of the Gaussian distribution.

## Requirement

C++11

## Setup

    ./waf configure
    ./waf

## How to run

    ./bin/default/burstyphrase -T [TARGET_FILE_NAME] -R [REFERENCE_FILE_NAME] > [OUTPUT_FILE_NAME]

## Input Format

Both [TARGET_FILE_NAME] and [REFERENCE_FILE_NAME] are sets of text documents with delimiters.

Each input document has a header starting by ASCII code "0x02" and ending by "0x03". This header acts as a delimiter for documents.

## Set parameters

`./bin/default/burstyphrase` and see help.

`-r` corresponds to 1+\epsilon and `-z` to \theta in the reference paper.

## Future work

To prepare sample input (othar than Twitter data).

## Reference

Masumi Shirakawa, Takahiro Hara and Takuya Maekawa: Never Abandon Minorities: Exhaustive Extraction of Bursty Phrases on Microblogs Using Set Cover Problem, Conference on Empirical Methods in Natural Language Processing (EMNLP 2017) (Sept. 2017, to appear).
