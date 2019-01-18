# Arraylets
Contiguous Memory Arraylets Double Mapping

Approach 1: 

1.	In order to simulate the GC heap, we used a shm_get call to reserve a shared memory space. After that we call ftruncate to set the desired size to be allocated, say 256MB of memory. Finally we call mmap on the file descriptor returned by shm_get to reserve the memory space , in a random location (NULL). Also PROT_READ and PROT_WRITE was passed in to mmap so we could write to the heap, and of course we passed MAP_SHARED. 
2.	Next we simulated arraylets by randomly picking locations in the heap and storing numbers of size of 2 pages. So if the size of the system page is 4KB we would store 8KB for each arraylet. At this point we have our representation of the heap along with the array lets scattered across memory. 
3.	On this step we create/reserve a contiguous memory space in order to double map the arraylets. We pre calculate the total size of all arraylets to reserve such space. We used and anonymous mmap with PROT_READ, PROT_WRITE and MAP_SHARED flags. 
4.	Lastly, we had to make one call to mmap for each arraylet because one call to mmap would not work for all of them since mmap always allocate a contiguous block of memory. In this approach only one file descriptor is used (different from approach 2) which relates to the heap. For each mmap call we pass in this file descriptor which was created on step 1 as well as each arraylet offset into the heap. For example: 

	Heap representation:
	
     <pre>
	0xFF0000    0xFF0000 + 40960<br />
	[          |7777777|         |333**3333|       |2222**2|      ]<br />

	Contiguous mem:	[333**333377777772222**2]<br />
	</pre>
	
	Heap start address: 0xFF0000<br />
	Arraylet |7777777| offset: 40960 bytes (page alligned)<br />
	Arraylet location = 0xFF0000 + 40960<br />

	By doing so we were able to double map every arraylet into the contiguous block of memory. As a proof of it, we tried changing the contiguous block of memory by writing asterisks in place of the numbers and as expected we were able to observe the changes in the heap as well. 

Approach 2:

1.	Unlike approach 1, in order to simulate the heap we only made a call to mmap without using shm_get. In this case we did not use a file descriptor and instead of passing PROT_READ and PROT_WRITE we passed PROT_NONE to indicate that nothing can be read written or executed in the heap before actually allocating a proper region for that (in this case the arraylets). 
2.	To allocate the arraylets we used one file descriptor for each of them. We do so by calling shm_get to then call ftruncate with the size of the arraylet e.g. 8KB; finally, we call mmap with flags PROT_READ, PROT_WRITE, MAP_SHARED and MAP_FIXED because we want to specifically put arraylets in locations (random) in the heap. With this approach only the arraylet space can be read written to, while the rest of the heap stays protected. Furthermore, if there are 500 arraylets we would require 500 file descriptors in this approach. This is a bottleneck because systems have a hard limit on how many file descriptors can be used at a time, e.g. the system we used had a 253 as a limit. 
3.	Step 3 is exactly the same as approach 1’s step 3 where we create/reserve a contiguous block of memory to double map the arraylets from the heap. 
4.	Lastly, unlike the previous approach where we used the arraylets offset into the heap, in this approach we use each arraylet file descriptor to mmap into the contiguous block of memory created in step 3. Therefore, instead of passing in the heap address to mmap we pass the contiguous block of memory address along with arraylet file descriptors. The end result is the same as before where the double mapping is successful. 


Cons approach 2:<br />
•	Hard limit of number of file descriptors (ulimit) that can be used at a time 

Cons approach 1:<br />
•	Associate a file descriptor to the entire heap, as a result we would have to make the entire heap as a big chunk of shared memory. 

