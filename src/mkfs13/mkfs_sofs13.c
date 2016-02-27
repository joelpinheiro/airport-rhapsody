

    /**
     *  \file mkfs_sofs13.c (implementation file)
     *
     *  \brief The SOFS13 formatting tool.
     *
     *  It stores in predefined blocks of the storage device the file system metadata. With it, the storage device may be
     *  envisaged operationally as an implementation of SOFS13.
     *
     *  The following data structures are created and initialized:
     *     \li the superblock
     *     \li the table of inodes
     *     \li the mapping table cluster-to-inode
     *     \li the data zone
     *     \li the contents of the root directory seen as empty.
     *
     *  SINOPSIS:
     *  <P><PRE>                mkfs_sofs13 [OPTIONS] supp-file
     *
     *                OPTIONS:
     *                 -n name --- set volume name (default: "SOFS13")
     *                 -i num  --- set number of inodes (default: N/8, where N = number of blocks)
     *                 -z      --- set zero mode (default: not zero)
     *                 -q      --- set quiet mode (default: not quiet)
     *                 -h      --- print this help.</PRE>
     *
     *  \author Artur Carneiro Pereira - September 2008
     *  \author Miguel Oliveira e Silva - September 2009
     *  \author António Rui Borges - September 2010 - September 2013
     */

    #include <inttypes.h>
    #include <stdbool.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <unistd.h>
    #include <libgen.h>
    #include <sys/stat.h>
    #include <sys/types.h>
    #include <string.h>
    #include <time.h>
    #include <errno.h>

    #include "sofs_const.h"
    #include "sofs_buffercache.h"
    #include "sofs_superblock.h"
    #include "sofs_inode.h"
    #include "sofs_direntry.h"
    #include "sofs_basicoper.h"
    #include "sofs_basicconsist.h"

    /* Allusion to internal functions */

    static int fillInSuperBlock (SOSuperBlock *p_sb, uint32_t ntotal, uint32_t itotal, uint32_t ctinmblktotal, uint32_t fcblktotal,
                                         uint32_t nclusttotal, unsigned char *name);
    static int fillInINT (SOSuperBlock *p_sb);
    static int fillInCIT (SOSuperBlock *p_sb);
    static int fillInRootDir (SOSuperBlock *p_sb);
    static int fillInBitMapT (SOSuperBlock *p_sb, int zero);
    static int checkFSConsist (void);
    static void printUsage (char *cmd_name);
    static void printError (int errcode, char *cmd_name);

    /* The main function */

    int main (int argc, char *argv[])                                    /* insert your code here */
    {
      char *name = "SOFS13";                         /* volume name */
      uint32_t itotal = 0;                           /* total number of inodes, if kept, set value automatically */
      int quiet = 0;                                 /* quiet mode, if kept, set not quiet mode */
      int zero = 0;                                  /* zero mode, if kept, set not zero mode */

      /* process command line options */

      int opt;                                       /* selected option */

      do
      { switch ((opt = getopt (argc, argv, "n:i:qzh")))
        { case 'n': /* volume name */
                    name = optarg;
                    break;
          case 'i': /* total number of inodes */
                    if (atoi (optarg) < 0)
                       { fprintf (stderr, "%s: Negative inodes number.\n", basename (argv[0]));
                         printUsage (basename (argv[0]));
                         return EXIT_FAILURE;
                       }
                    itotal = (uint32_t) atoi (optarg);
                    break;
          case 'q': /* quiet mode */
                    quiet = 1;                       /* set quiet mode for processing: no messages are issued */
                    break;
          case 'z': /* zero mode */
                    zero = 1;                        /* set zero mode for processing: the information content of all free
                                                        data clusters are set to zero */
                    break;
          case 'h': /* help mode */
                    printUsage (basename (argv[0]));
                    return EXIT_SUCCESS;
          case -1:  break;
          default:  fprintf (stderr, "%s: Wrong option.\n", basename (argv[0]));
                    printUsage (basename (argv[0]));
                    return EXIT_FAILURE;
        }
      } while (opt != -1);
      if ((argc - optind) != 1)                      /* check existence of mandatory argument: storage device name */
         { fprintf (stderr, "%s: Wrong number of mandatory arguments.\n", basename (argv[0]));
           printUsage (basename (argv[0]));
           return EXIT_FAILURE;
         }

      /* check for storage device conformity */

      char *devname;                                 /* path to the storage device in the Linux file system */
      struct stat st;                                /* file attributes */

      devname = argv[optind];
      if (stat (devname, &st) == -1)                 /* get file attributes */
         { printError (-errno, basename (argv[0]));
           return EXIT_FAILURE;
         }
      if (st.st_size % BLOCK_SIZE != 0)              /* check file size: the storage device must have a size in bytes
                                                        multiple of block size */
         { fprintf (stderr, "%s: Bad size of support file.\n", basename (argv[0]));
           return EXIT_FAILURE;
         }


      /* evaluating the file system architecture parameters
       * full occupation of the storage device when seen as an array of blocks supposes the equation bellow
       *
       *    NTBlk = 1 + sige(NTClt/BPB) + sige(NTClt/RPB) + NBlkTIN + NTClt*BLOCKS_PER_CLUSTER
       *
       *    where NTBlk means total number of blocks
       *          NTClt means total number of clusters of the data zone
       *          BPB means total number of references to clusters (bitmap) which can be stored in a block
       *          RPB means total number of references to clusters which can be stored in a block
       *          NBlkTIN means total number of blocks required to store the inode table
       *          BLOCKS_PER_CLUSTER means number of blocks which fit in a cluster
       *          sige (.) means the smallest integer greater or equal to the argument
       *
       * has integer solutions
       * this is not always true, so a final adjustment may be made to the parameter NBlkTIN to warrant this
       * furthermore, since the equation is non-linear, the procedure to solve it requires several steps
       * (in this case three)
       */

      uint32_t ntotal;                               /* total number of blocks */
      uint32_t iblktotal;                            /* number of blocks of the inode table */
      uint32_t nclusttotal;                          /* total number of clusters */
      uint32_t fcblktotal;                           /* number of blocks of the free clusters table (bitmap) */
      uint32_t ctinmblktotal;                        /* number of blocks of the cluster to inode mapping table */
      uint32_t tmp;                                  /* temporary variable */

      ntotal = st.st_size / BLOCK_SIZE;
      if (itotal == 0) itotal = ntotal >> 3;
      if ((itotal % IPB) == 0)
         iblktotal = itotal / IPB;
         else iblktotal = itotal / IPB + 1;
                                                     /* step number 1 */
      tmp = (ntotal - 1 - iblktotal) / BLOCKS_PER_CLUSTER;
      if ((tmp % BITS_PER_BLOCK) == 0)
         fcblktotal = tmp / BITS_PER_BLOCK;
         else fcblktotal = tmp / BITS_PER_BLOCK + 1;
      if ((tmp % RPB) == 0)
              ctinmblktotal = tmp / RPB;
         else ctinmblktotal = tmp / RPB + 1;
                                                     /* step number 2 */
      nclusttotal = (ntotal - 1 - iblktotal - fcblktotal - ctinmblktotal) / BLOCKS_PER_CLUSTER;
      if ((nclusttotal % BITS_PER_BLOCK) == 0)
         fcblktotal = nclusttotal / BITS_PER_BLOCK;
         else fcblktotal = nclusttotal / BITS_PER_BLOCK + 1;
      if ((nclusttotal % RPB) == 0)
              ctinmblktotal = nclusttotal / RPB;
         else ctinmblktotal = nclusttotal / RPB + 1;
                                                     /* step number 3 */
      if (((nclusttotal % BITS_PER_BLOCK) != 0) && ((nclusttotal % RPB) != 0))
         { if ((ntotal - 1 - iblktotal - fcblktotal - ctinmblktotal - nclusttotal * BLOCKS_PER_CLUSTER) >=
               BLOCKS_PER_CLUSTER)
              nclusttotal += 1;
         }
                                                     /* final adjustment */
      iblktotal = ntotal - 1 - fcblktotal - ctinmblktotal - nclusttotal * BLOCKS_PER_CLUSTER;
      itotal = iblktotal * IPB;

      /* formatting of the storage device is going to start */

      SOSuperBlock *p_sb;                            /* pointer to the superblock */
      int status;                                    /* status of operation */

      if (!quiet)
         printf("\e[34mInstalling a %"PRIu32"-inodes SOFS13 file system in %s.\e[0m\n", itotal, argv[optind]);

      /* open a buffered communication channel with the storage device */

      if ((status = soOpenBufferCache (argv[optind], BUF)) != 0)
         { printError (status, basename (argv[0]));
           return EXIT_FAILURE;
         }

      /* read the contents of the superblock to the internal storage area
       * this operation only serves at present time to get a pointer to the superblock storage area in main memory
       */

      if ((status = soLoadSuperBlock ()) != 0)
         return status;
      p_sb = soGetSuperBlock ();

      /* filling in the superblock fields:
       *   magic number should be set presently to 0xFFFF, this enables that if something goes wrong during formating, the
       *   device can never be mounted later on
       */

      if (!quiet)
         { printf ("Filling in the superblock fields ... ");
           fflush (stdout);                          /* make sure the message is printed now */
         }

      if ((status = fillInSuperBlock (p_sb, ntotal, itotal, ctinmblktotal, fcblktotal, nclusttotal, (unsigned char *) name)) != 0)
         { printError (status, basename (argv[0]));
           soCloseBufferCache ();
           return EXIT_FAILURE;
         }

      if (!quiet) printf ("done.\n");

      /* filling in the inode table:
       *   only inode 0 is in use (it describes the root directory)
       */

      if (!quiet)
         { printf ("Filling in the table of inodes ... ");
           fflush (stdout);                          /* make sure the message is printed now */
         }

      if ((status = fillInINT (p_sb)) != 0)
         { printError (status, basename (argv[0]));
           soCloseBufferCache ();
           return EXIT_FAILURE;
         }

      if (!quiet) printf ("done.\n");

      /* filling in the cluster-to-inode mapping table:
       *   only data cluster 0 has been allocated (it stores the contents of the root directory)
       *   so only the first element of the table is equal to inode 0, all the others are equal to NULL_INODE
       */

      if (!quiet)
         { printf ("Filling in the cluster-to-inode mapping table ... ");
           fflush (stdout);                          /* make sure the message is printed now */
         }


      // Joel Pinheiro
      //soStoreSuperBlock();
      // exit(EXIT_FAILURE);

      if ((status = fillInCIT (p_sb)) != 0)
         { printError (status, basename (argv[0]));
           soCloseBufferCache ();
           return EXIT_FAILURE;
         }

      if (!quiet) printf ("done.\n");

      /* filling in the contents of the root directory:
       *   the first 2 entries are filled in with "." and ".." references
       *   the other entries are kept empty
       */

      if (!quiet)
         { printf ("Filling in the contents of the root directory ... ");
           fflush (stdout);                          /* make sure the message is printed now */
         }

      if ((status = fillInRootDir (p_sb)) != 0)
         { printError (status, basename (argv[0]));
           soCloseBufferCache ();
           return EXIT_FAILURE;
         }

      if (!quiet) printf ("done.\n");

      /*
       * create the bitmap table to free data clusters
       *   only data cluster 0 has been allocated (it stores the contents of the root directory)
       * zero fill the remaining data clusters if full formating was required:
       *   zero mode was selected
       */

      if (!quiet)
         { printf ("Filling in the contents of the bitmap table to free data clusters ... ");
           fflush (stdout);                          /* make sure the message is printed now */
         }

      if ((status = fillInBitMapT (p_sb, zero)) != 0)
         { printError (status, basename (argv[0]));
           soCloseBufferCache ();
           return EXIT_FAILURE;
         }

      if (!quiet) printf ("done.\n");

      /* magic number should now be set to the right value before writing the contents of the superblock to the storage
         device */

      p_sb->magic = MAGIC_NUMBER;
      if ((status = soStoreSuperBlock ()) != 0)
         return status;

      /* check the consistency of the file system metadata */

      if (!quiet)
         { printf ("Checking file system metadata... ");
           fflush (stdout);                          /* make sure the message is printed now */
         }

      if ((status = checkFSConsist ()) != 0)
         { fprintf(stderr, "error # %d - %s\n", -status, soGetErrorMessage (-status));
           soCloseBufferCache ();
           return EXIT_FAILURE;
         }

      if (!quiet) printf ("done.\n");

      /* close the unbuffered communication channel with the storage device */

      if ((status = soCloseBufferCache ()) != 0)
         { printError (status, basename (argv[0]));
           return EXIT_FAILURE;
         }

      /* that's all */

      if (!quiet) printf ("Formating concluded.\n");

      return EXIT_SUCCESS;

    } /* end of main */

    /*
     * print help message
     */

    static void printUsage (char *cmd_name)
    {
      printf ("Sinopsis: %s [OPTIONS] supp-file\n"
              "  OPTIONS:\n"
              "  -n name --- set volume name (default: \"SOFS13\")\n"
              "  -i num  --- set number of inodes (default: N/8, where N = number of blocks)\n"
              "  -z      --- set zero mode (default: not zero)\n"
              "  -q      --- set quiet mode (default: not quiet)\n"
              "  -h      --- print this help\n", cmd_name);
    }

    /*
     * print error message
     */

    static void printError (int errcode, char *cmd_name)
    {
      fprintf(stderr, "%s: error #%d - %s\n", cmd_name, -errcode,
              soGetErrorMessage (-errcode));
    }

      /* filling in the superblock fields:
       *   magic number should be set presently to 0xFFFF, this enables that if something goes wrong during formating, the
       *   device can never be mounted later on
       */

    static int fillInSuperBlock (SOSuperBlock *p_sb, uint32_t ntotal, uint32_t itotal, uint32_t ctinmblktotal, uint32_t fcblktotal,
                                         uint32_t nclusttotal, unsigned char *name)
    {
                    /* header */
                    // Função criada por Rui Monteiro

                    int i;          // variavel usada nos ciclos for
                    // int status;  // faz o return da funcao store do superblock
                    int iblktotal;

                    if(p_sb == NULL)
                      return -EINVAL;

                    p_sb->magic = 0xFFFF; /* magic number - file system id number */
                    p_sb->version = VERSION_NUMBER; /* version number */
                    strncpy((char*)p_sb->name, (char*) name, PARTITION_NAME_SIZE + 1); /* volume name */
                    p_sb->name[PARTITION_NAME_SIZE] = '\0';
                    p_sb->ntotal = ntotal; /* total number of blocks in the device */
                    p_sb->mstat = PRU; // flag signaling if the file system was properly unmounted the last time it was mounted. PRU - Properly Unmounted

                    // Tabela i-Node
                    p_sb->itable_start = 1;
                    iblktotal = itotal/IPB;
                    p_sb->itable_size = iblktotal;
                    p_sb->itotal=itotal;            //nº de i-nodes
            p_sb->ifree = itotal - 1;
            p_sb->ihead = 1;
            p_sb->itail = itotal -1 ;

                    p_sb->ciutable_start = p_sb->itable_start + p_sb->itable_size;
                    p_sb->ciutable_size = ctinmblktotal;


                    p_sb->fctable_start = p_sb->ciutable_start + p_sb->ciutable_size;;      //inicia a seguir aos blocos ocupados pela tabela de inodes
                    p_sb->fctable_size = fcblktotal;
                    p_sb->fctable_pos = 1;
                    /* zona de dados */
                    p_sb->dzone_start = p_sb->fctable_start + p_sb->fctable_size;  /* Starting position of data clusters*/
                    p_sb->dzone_total = nclusttotal;        /* total number of data clusters */
                    p_sb->dzone_free=nclusttotal - 1;  /* number of free data clusters */

                    /* retrieval cache of references to free data clusters */

                    // p_sb->dzone_free = nclusttotal-1; /* number of free data clusters */
                    //p_sb->dzone_retriev.cache_idx=DZONE_CACHE_SIZE; /* retrieval cache of references to free data clusters */


                    p_sb->dzone_retriev.cache_idx = DZONE_CACHE_SIZE;       //limpar cache de retirada - quando esta vazia, apontar para a posicao a seguir a ultima

                    for (i = 0; i < DZONE_CACHE_SIZE; i++)          //limpar a cache de retirada
                    {
                            p_sb->dzone_retriev.cache[i] = NULL_CLUSTER;
                    }

                    p_sb->dzone_insert.cache_idx =  0;      //limpar cache de insercao - quando esta vazia, apontar para a posicao a seguir a ultima

                    for (i = 0; i < DZONE_CACHE_SIZE; i++)          //limpar a cache de insercao
                    {
                            p_sb->dzone_insert.cache[i] = NULL_CLUSTER;
                    }

                    /*if((nclusttotal%RPB)==0)
                            p_sb->fctable_size = nclusttotal/RPB;           //O tamanho da tabela equivalera ao total de blocos na free cluster table.
                    else
                            p_sb->fctable_size = nclusttotal/RPB+1;*/

                    for (i=0; i<BLOCK_SIZE-PARTITION_NAME_SIZE - 1 - 18 * sizeof(uint32_t) - 2 * sizeof(struct fCNode); i++)
                    {
                            p_sb->reserved[i] = 0xEE;       // qualquer valor
                    }

                    //guarda o super bloco no File System e retorna o valor do erro (caso ocorra algum)
    /*
                    if ((status = soStoreSuperBlock ()) != 0)
                            return status;*/
                    return 0;

    }

    /*
     * filling in the inode table:
     *   only inode 0 is in use (it describes the root directory)
     */

    static int fillInINT (SOSuperBlock *p_sb)
    {
        if(p_sb == NULL)
        return -EINVAL;				/*verificação a cabeça se o ponteiro contém conteudo válido*/

	uint32_t s;
	uint32_t i;	/*Variavel que vai identificar cada inode dentro dos blocos */
	uint32_t blk;	/*Variavel que vai identificar cada bloco */
	uint32_t k;	/*Variavel que vai identificar cada indice do vector .d[] de cada inode */

	SOInode *inode; /*Criacao do bloco de inodes, tendo cada bloco IPB inodes */

	for (blk = 0; blk < p_sb->itable_size; blk++){			/*preenchimento da tabela de inodes com inodes livre*/
	  if((s=soLoadBlockInT(blk)))
           return s;
         inode=soGetBlockInT();
		for (i = 0; i < IPB; i++){

     			inode[i].mode = INODE_FREE;
     			inode[i].refcount = 0;
     			inode[i].owner = 0;
     			inode[i].group = 0;
     			inode[i].size = 0;
     			inode[i].clucount = 0;


		/*Atribuicao aos ponteiros .prev e .next os numeros do inode anterior e seguinte, respectivamente. Criação de uma varialvel node */

		uint32_t node = IPB * blk + i;
		inode[i].vD1.prev = node-1;
		inode[i].vD2.next = node+1;


			for (k=0; k < N_DIRECT; k++){			/*Preenchimento da tabela de referencias directas, neste caso com null_cluster visto que estes agora estão vazias*/

			inode[i].d[k]= NULL_CLUSTER;
			}

			inode[i].i1= NULL_CLUSTER;				/*referencias inderectas tbm vazias*/
			inode[i].i2= NULL_CLUSTER;				/*referencias duplamente inderectas tbm vazias*/




			if(node==0){						/*nó zero que contém o directorio raiz*/

			inode[i].mode= (INODE_RD_OTH | INODE_WR_OTH  | INODE_EX_OTH  | INODE_RD_GRP  | INODE_WR_GRP | INODE_EX_GRP | INODE_RD_USR | INODE_WR_USR | INODE_EX_USR | INODE_DIR);
			inode[i].refcount = 2;
			inode[i].owner = getuid();
     			inode[i].group = getgid();
     			inode[i].size = CLUSTER_SIZE;
     			inode[i].clucount = 1;
			inode[i].vD1.atime = time(NULL);
			inode[i].vD2.mtime = time(NULL);
			inode[i].d[0]= 0;
			}

			if(node==1){						/*Caso do 1º inode livre*/

			inode[i].vD1.prev= NULL_INODE;
			inode[i].vD2.next= node+1;


			}

			if(node==p_sb->itotal-1){				/*Caso para o ultimo Inode livre da lista*/
			inode[i].vD1.prev= node-1;
			inode[i].vD2.next= NULL_INODE;

			}

		}
	 if ((s = soStoreBlockInT ()) != 0)
           return s;

	}

	return 0;
    }

    /* filling in the cluster-to-inode mapping table:
     *   only data cluster 0 has been allocated (it stores the contents of the root directory)
     *   so only the first element of the table is equal to inode 0, all the others are equal to NULL_INODE
     */

    static int fillInCIT (SOSuperBlock *p_sb)
    {
    	uint32_t ref,p_nBlk, p_offset;
    	uint32_t* bits;
    	int status;

    	// Dá ao primeiro data cluster o valor 0
    	if((status = soLoadBlockCTInMT(0)) != 0)
    		return status;
    	bits = soGetBlockCTInMT();
    	bits[0] = 0;

    	// poe todos os data clusters a NULL
    	for(ref = 1; ref < p_sb->dzone_total; ref++)
    	{

    		if((status = soConvertRefCInMT(ref,&p_nBlk,&p_offset))!= 0)
    				return status;

    		if((p_offset) == 0)
    		{
    			if((status = soStoreBlockCTInMT())!= 0)
    				return status;
    			if((status = soLoadBlockCTInMT(p_nBlk)) != 0)
    			{
    				return status;
    			}
    				bits = soGetBlockCTInMT();
    		}
    		bits[p_offset] = NULL_INODE;

    	}

    	// preenche o resto do bloco com 0xFFFFFFFE
    	p_offset++;
    	for(; p_offset<RPB; p_offset++)
    	{
    		bits[p_offset] = 0xFFFFFFFE;
    	}

    	// Store block
    	if(( status = soStoreBlockCTInMT()) != 0)
    		return status;

    	return 0;
    }
    /*
     * filling in the contents of the root directory:
         the first 2 entries are filled in with "." and ".." references
         the other entries are empty
     */

    static int fillInRootDir (SOSuperBlock *p_sb)
    {
            // Função criada por José Mendes

            int i;

            SODirEntry dir[DPC]; /* creates the table */

            /* creates "." reference */
            strncpy((char *) dir[0].name, ".", MAX_NAME + 1);
            dir[0].nInode = 0;

            /* creates ".." reference */
            strncpy((char *) dir[1].name, "..", MAX_NAME + 1);
            dir[1].nInode = 0;

            /* clear the root entries */
            for(i = 2 ; i<DPC; i++)
            {
                    strncpy((char *) dir[i].name, "", MAX_NAME + 1);
                    dir[i].nInode = NULL_INODE;
            }

            return soWriteCacheCluster(p_sb->dzone_start,&dir); /* writes the cluster */
    }


      /*
       * create the bitmap table to free data clusters
       *   only data cluster 0 has been allocated (it stores the contents of the root directory)
       * zero fill the remaining data clusters if full formating was required:
       *   zero mode was selected
       */

static int fillInBitMapT (SOSuperBlock *p_sb, int zero)
{
	// Função criada por João Ribeiro
  uint32_t i, bloco, byteoff, bitoff;
  int error;
  SODataClust clt;
  unsigned char *p_blck;

  soConvertRefBMapT(0, &bloco, &byteoff, &bitoff);	//gets ref to the first element of bmap table

  if((error = soLoadBlockBMapT(bloco)) != 0)		// loads the first block of bitmap
    return error;

  p_blck = soGetBlockBMapT();

  memset(p_blck, 0, BLOCK_SIZE);
  p_blck[0] = 0x7F;				// sets the first bit 0

  if((error = soStoreBlockBMapT()) != 0)
    return error;

  for(i = 1; i < p_sb->dzone_total; i++)
  {
    soConvertRefBMapT(i, &bloco, &byteoff, &bitoff);	// gets references to the bmap current block
    if((error = soLoadBlockBMapT(bloco)) != 0)
      return error;

    p_blck = soGetBlockBMapT();

    if(byteoff == 0 && bitoff == 0) memset(p_blck, 0, BLOCK_SIZE); // if new block sets 0s   

    p_blck[byteoff] |= (1 << (7-bitoff));		// sets current bit not free

    if(zero)						// sets cluster data to 0
    {
      memset(clt.data, 0, BSLPC);
      if((error = soWriteCacheCluster((p_sb->dzone_start + i * BLOCKS_PER_CLUSTER), &clt)) != 0)
        return error;
    }
    if((error = soStoreBlockBMapT()) != 0)		// saves the bmap block
      return error;
  }

  return 0;
}
    /*
       check the consistency of the file system metadata
     */

    static int checkFSConsist (void)
    {
      SOSuperBlock *p_sb;                            /* pointer to the superblock */
      SOInode *inode;                                /* pointer to the contents of a block of the inode table */
      int stat;                                      /* status of operation */

      /* read the contents of the superblock to the internal storage area and get a pointer to it */

      if ((stat = soLoadSuperBlock ()) != 0) return stat;
      p_sb = soGetSuperBlock ();

      /* check superblock and related structures */

    if ((stat = soQCheckSuperBlock (p_sb)) != 0) return stat;

      /* read the contents of the first block of the inode table to the internal storage area and get a pointer to it */

    if ((stat = soLoadBlockInT (0)) != 0) return stat;
    inode = soGetBlockInT ();

      /* check inode associated with root directory (inode 0) and the contents of the root directory */

    if ((stat = soQCheckInodeIU (p_sb, &inode[0])) != 0) return stat;
    if ((stat = soQCheckDirCont (p_sb, &inode[0])) != 0) return stat;

      /* everything is consistent */

      return 0;
    }

