# Verkle Trees
This is the accompanying code for our research project for Cryptocurrencies & Distributed Ledgers Fall-22 Course. 

## Introduction
We implement Verkle, Patricia-Merkle, and Binary-Merkle in C++. We compare the proof sizes, proof generation time, and proof verification time for varying key sizes for all the three trees. 

We also compare two vector commitment schemes KZG and IPA in our Verkle Trees. For each of them, we compare multiple width versions of them. 

## Results
<img src="imgs/proof_size.png">
<img src="imgs/gen_proof_time.png">
<img src="imgs/ver_proof_time.png">

## Authors
- Pranav Jangir [pj2251@nyu.edu]
- Animesh Ramesh [ar8006@nyu.edu]
- Arun Patro [akp7833@nyu.edu]

[![forthebadge](https://forthebadge.com/images/badges/works-on-my-machine.svg)](https://forthebadge.com)

Verkle tree implementation in C++.

Dependencies : 

blst library
c-kzg library
GNU GMP library