#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>


typedef struct DiskioDescriptorRec_ {
	unsigned char	ctrl_lun, dev_lun;
	unsigned short  status;
	unsigned long   pbuf;
	unsigned long   blk_num;
	unsigned short  blk_cnt;
    unsigned char   flag;
    unsigned char   addr_mod;
} DiskioDescriptorRec, *DiskioDescriptor;

#ifdef __rtems__
static inline void read_blk(DiskioDescriptor d, int blkno)
{
	d->blk_num = blkno;
	__asm__ __volatile__("mr 3,%0; mr 10,%1; sc"::"r"(d),"r"(0x10):"r3","r10");	
}
#else
#define creat(nm,mod) 0
#define write(fd,buf,s) (s)
#define close(fd)       (0)
static inline void read_blk(DiskioDescriptor d, int blkno)
{
	read(d->dev_lun, (void*)d->pbuf, 512); /* cheat, don't seek! */
}
#endif

static unsigned long
o2ul(char *chpt, int len)
{
unsigned long rval;
	for (rval = 0; --len >= 0; chpt++) {
		if ( *chpt < '0' || *chpt > '9' )
			continue;
		rval = 8*rval + (*chpt-'0');
	}
	return rval;
}

static int tar_chksum(char *chpt)
{
int rval = 0, i;
	for ( i=0; i< 512; i++ )
		rval += 0xff & chpt[i];
	for ( i=148; i<156; i++)
		rval += (0xff & ' ') - (0xff & chpt[i]);
	return rval;
}

/* definitions from cpukit/libfs/src/imfs/imfs_load_tar */

#define LF_OLDNORMAL  '\0'     /* Normal disk file, Unix compatible */
#define LF_NORMAL     '0'      /* Normal disk file                  */
#define LF_LINK       '1'      /* Link to previously dumped file    */
#define LF_SYMLINK    '2'      /* Symbolic link                     */
#define LF_CHR        '3'      /* Character special file            */
#define LF_BLK        '4'      /* Block special file                */
#define LF_DIR        '5'      /* Directory                         */
#define LF_FIFO       '6'      /* FIFO special file                 */
#define LF_CONFIG     '7'      /* Contiguous file                   */

int
loadTarImg(int verb, int lun)
{
char          		buf[512];
int           		blkno, chks, i;
unsigned long		mode,size;
unsigned char		type;
DiskioDescriptorRec d = {0};

	d.dev_lun = lun;
	d.blk_cnt = 1;
	d.pbuf    = (unsigned long)buf;

	blkno = 0;

	do {

		read_blk(&d, blkno++);

		if ( strncmp(buf+257, "ustar  ", 7) )
			break;

		switch (buf[156]) {
			case LF_OLDNORMAL: type = ' '; break;
			case LF_NORMAL   : type = ' '; break;
			case LF_LINK     : type = '-'; break;
			case LF_SYMLINK  : type = 's'; break;
			case LF_CHR      : type = 'c'; break;
			case LF_BLK      : type = 'b'; break;
			case LF_DIR      : type = 'd'; break;
			case LF_FIFO     : type = 'p'; break;
			case LF_CONFIG   :
			default:           type = '?'; break;
		}

		mode = o2ul(buf+100, 8);
		size = o2ul(buf+124,12);
		chks = (int)o2ul(buf+148,8);
		i    = tar_chksum(buf);

		if ( verb ) {
			printf("Mode: 0%012o, size %9i %c - %s\n", mode, size, type, buf);
		}
		if ( i!=chks ) {
			fprintf(stderr,"Checksum mismatch (read %i, calculated %i)\n", chks, i);
			return -1;
		}
		if ( ' ' == type ) {
			if ( (i = creat(buf, mode)) < 0 ) {
				perror("creating file");
				return -1;
			}
			while ( size > 0 ) {
				chks = size >= 512 ? 512 : size;
				read_blk(&d, blkno++);
				if ( chks!=write(i,buf,chks) ) {
					perror("writing block");
					close(i);
					return -1;
				}
				size -= chks;
			}
			if ( close(i) ) {
				perror("close");
				return -1;
			}
		} else if ( 'd' == type ) {
			mkdir(buf, S_IRWXU | S_IRWXG | S_IRWXO);
		}
	} while (1);

	return 0;
}

#ifndef __rtems__
int
main(int argc, char **argv)
{
	loadTarImg(1,0);
}
#endif
