/*************************************************************************
	> File Name: bitfield.c
	> Author: ma6174
	> Mail: ma6174@163.com 
	> Created Time: 2015年11月03日 星期二 18时21分04秒
 ************************************************************************/

#include<stdio.h>
#include<unistd>
#include<string.h>
#include<malloc.h>
#include<fcntl.h>
#include<sys/stat.h>
#include"parse_metafile.h"
#include"bitfield.h"


extern int pieces_length;
extern char *file_name;

Bitmap *bitmap=NULL;				//指向位图
int		download_piece_num=0;		//当前已下载的piece数


int create_bitfield()
{
	bitmap=(Bitmap)*malloc(sizeof(Bitmap));
	if(bitmap==NULL){
		printf("allocate memory for bitmap failed\n");
		return -1;
	}
	//pieces_length除以20即为总的piece数
	bitmap->valid_length=pieces_length/20;
	bitmap->bitfield_length=pieces_length/20/8;
	if((pieces_length/20)%8!=0)	bitmap->bitfield_length++;

	bitmap->bitfield=(unsigned char*)malloc(bitmap->bitfield_length);
	if(bitmap->bitfield==NULL){
		printf("alloc memory for bitmap->bitfield failed\n");
		if(bitmap!=NULL)	free(bitmap);
		return -1;
	}

	char bitmapfile[64];
	sprintf(bitmapfile,"%ddbitmap",pieces_length);
	int i;
	FILE *fp=fopen(bitmapfile,"rb");
	if(fp==NULL){	//若打开文件失败，说明开始的是一个全新的下载
		memset(bitmap->bitfield,0,bitmap->bitfield_length);
	}
	else{
		fseek(fp,0,SEEK_SET);
		for(i=0;i<bitmap->field_length;i++)
			(bitmap->bitfield)[i]=fgetc(fp);
		fclose(fp);
		//给download_piece_num赋新的初值
		download_piece_num=get_download_piece_num();
	}
	return 0;
};


int get_bit_value(Bitmap *bitmap,int index)
{
	int ret;
	int byte_index;
	unsigned char byte_value;
	unsigned char inner_byte_index;

	if(bitmap==NULL||index>=bitmap->valid_length)	return -1;
	byte_index=index/8;
	byte_value=bitmap->bitfield[byte_index];
	inner_byte_index=index%8;

	byte_value=byte_value>>(7-inner_byte_index);
	if(byte_value%2==0)	ret=0;
	else ret=1;

	return ret;
};


int all_zero(Bitmap *bitmap)
{
	if(bitmap->bitfield==NULL)	return -1;
	memset(bitmap->bitfield,0,bitmap->bitfield_length);
	return 0;
};


int all_set(Bitmap *bitmap);


void release_memory_in_bitfield()
{
	if(bitmap->bitfield!=NULL)	free(bitmap->bitfield);
	if(bitmap!=NULL)	free(bitmap);
}

int print_bitfield(Bitmap *bitmap);


int restore_bitmap()
{
	int fd;
	char bitmapfile[64];

	if((bitmap==NULL)||(file_name==NULL))	return -1;
	sprintf(bitmapfile,"%dbitmap",pieces_length);
	fd=open(bitmapfile,O_RDWR|O_CREAT|O_TRUNC,0666);
	if(fd<0)	return -1;
	write(fd,bitmap->bitfield,bitmap->bitfield_length);
	close(fd);

	return 0;
};


int is_interested(Bitmap *dst,Bitmap *src)
{
	unsigned char const_char[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
	unsigned char c1,c2;

	if(dst==NULL||src==NULL)	return -1;
	if(dst->bitfield==NULL||src->bitfield==NULL)	return -1;
	if(dst->bitfield_length!=src->bitfield_length||dst->valid_length!=src->valid_length)	return -1;
	//如果dst中某位为1，而src对应为0，则说明src对dst感兴趣
	for(i=0;i<dst->bitfield_length-1;i++){
		for(j=0;j<8;j++){	//比较某个字节的所有位
			c1=(dst->bitfield)[i]&const_char[j];//获取每一位的值
			c2=(src->bitfield)[i]&const_char[j];
			if(c1>0&&c2==0)	return 1;
		}
	}

	j=dst->valid_length%8;
	c1=dst->bitfield[dst->bitfield_length-1];
	c2=src->bitfield[src->bitfield_length-1];
	for(i=0;i<j;i++){	//比较位图的最后一个字节
		if((c1&const_char[i])>0&&(c2&const_char[i])==0)
			return 1;
	}
	return 0;
};


int get_download_piece_num()
{
	unsigned char const_char[8]={0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};
	int i,j;
	if(bitmap==NULL||bitmap->bitfield==NULL)	return 0;
	download_piece_num=0;

	for(i=0;i<bitmap->bitfield_length-1;i++){
		for(j=0;j<8;j++){
			if(((bitmap->bitfield)[i]&const_char[j])!=0)
				download_piece_num++;
		}
	}

	unsigned char c=(bitmap->bitfield)[i];	//c存放位图最后一个字节的值
	j=bitmap->valid_length%8;	//j是位图最后一个字节的有效位数
	for(i=0;i<j;i++){
		if(c&const_char[i]!=0)	download_piece_num++;
	}
	return download_piece_num;
};


//is_interested测试代码
void main()
{
	Bitmap map1,map2;
	unsigned char bf1[2]={0xa0,0xa0};	//位图每一位的值为10100000 10100000
	unsigned char bf2[2]={0xe0,0xe0};	//位图每一位的值为11100000 11100000

	map1.bitfield			=bf1;
	map1.bitfield_length	=2;
	map1.valid_length		=11;
	map2.bitfield			=bf2;
	map2.bitfield_length	=2;
	map2.valid_length		=11;

	int ret=is_interested(&map1,&map2);
	printf("%d\n",ret);
};
