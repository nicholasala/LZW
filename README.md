# LZW
C code of two version of Lempel-Ziv-Welch (LZW) compression algorithm:

  * fixed indexes
  * variable indexes
  
## Fixed indexes
Run instructions:

* Compile:
      
      > gcc LZW_IF.c

* Compress:
      
      > ./a.out	–c inFile	outFile

* Decompress:
      
      > ./a.out	–d inFile	outFile
      
## Variable indexes
Run instructions:

* Compile:
      
      > gcc LZW_IV.c -lm

* Compress:
      
      > ./a.out	–c inFile	outFile

* Decompress:
      
      > ./a.out	–d inFile	outFile
