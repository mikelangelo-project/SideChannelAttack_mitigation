# Cache SideChannelAttack Mitigation

For the last couple of years, a many articles have cropped up, claiming cache based attacks on virtual machines are practical and a real threat to cloud security. The attacks described in these articles vary in methods and tools, but are all based on stealing encryption data using cache timing attacks. Due to this looming threat, a need for a layer of defense, that can either detect, or even stop these attacks, is needed.

Some ways to defend against the attack have already been found, for example improving the protection on existing encryption algorithms. However we seek a solution that would protect all code from the attack, without requiring all programmers to build their code with the attack in mind. Therefore, we seek an answer on the Hypervisor level, in order to protect all our virtual machines regardless of whether their code is secure or not.

In this project we will Provide a defense on the hypervisor level that can either detect, or stop the attack.

## Requirments

Please copy the lib* files found in th binarys folder to "/usr/lib"

## Instalation

Download the files and run:
```
$ make
```
Inside the code files folder.
Cache_SCA_Mitigate Execuatble will be created.

## Instructions

Run the Execuatble by:
```
$ ./Cache_SCA_Mitigate
```
And follow the menu Instructions.

## Contact Information

For any further information please contact:

Dr. Niv Gilboa from Bgu : gilboan@bgu.ac.il

Dr. Gabriel Scalosub from BGU : sgabriel@bgu.ac.il

Mr. Gal Hershcovich from BGU : galhers@post.bgu.ac.il

## Acknowledgements

This project  has been conducted within the RIA [MIKELANGELO 
project](https://www.mikelangelo-project.eu) (no.  645402), started in January
2015, and co-funded by the European Commission under the H2020-ICT- 07-2014:
Advanced Cloud Infrastructures and Services programme.