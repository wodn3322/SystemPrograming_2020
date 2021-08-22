#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>	
#include <unistd.h>
#include <string.h>
#include <pthread.h>

typedef struct MatrixData
{
	int** mat;
	int ** filter;
	int i,j;
}MatrixData;

void conv(int** matrix,int** filter,int** aftermat,int row);
void *convCal(void *temp);
void maxPooling(int** matrix,int** aftermat,int row);
void *poolCal(void *temp);
int** makeAllocation(int** matrix, int row);
void freeAllocation(int** matrix, int row);

int main(int argc, char** argv){

	char** inputName = argv[1];
	char** outputName = argv[2];
	
	int row;
	int i,j;

	int** layerMatrix;
	int** filter;
	int** afterConvmat;
	int** afterMaxpmat;

	int readData;
	char readBUf[10];
	char readBUf2[2];
	char *writeBuf;
	char *buf = readBUf;
	char *buf2 =readBUf2;
	ssize_t ret;

	int fd = open(inputName,O_RDONLY);
	if(fd ==-1){
		perror("open");
		return 1;
	}

	while(1){
		ret=read(fd,buf,1);
		if(buf[0] != '\n'){
			if(readData == -1){
				perror("read");
				return -1;
			}
			buf += ret;
		}else{
			break;
		}
	}

	row = atoi(readBUf);

	layerMatrix=makeAllocation(layerMatrix,row);
	filter=makeAllocation(filter,3);
	afterConvmat=makeAllocation(afterConvmat,row-2);
	afterMaxpmat=makeAllocation(afterMaxpmat,(row-2)/2);

	filter[0][0]=-1;
	filter[0][1]=-1;
	filter[0][2]=-1;
	filter[1][0]=-1;
	filter[1][1]=8;
	filter[1][2]=-1;
	filter[2][0]=-1;
	filter[2][1]=-1;
	filter[2][2]=-1;

	i=0;
	j=0;
	readBUf2[2]='\0';
	readBUf2[3]='\0';

	while(1){
		if(i==row&&j==0){
				break;
		}
		fflush(stdin);
		if(j!=row){
			ret=read(fd,buf2,2);
			if(buf2[0] != '\n'){
				if(readData == -1){
					perror("read");
					return -1;
				}
				layerMatrix[i][j]=atoi(readBUf2);
				lseek(fd,1,SEEK_CUR);
			}
			j++;
		}else{
				i++;
				j=0;
		}
	}

	close(fd);

	conv(layerMatrix,filter,afterConvmat,row);
	maxPooling(afterConvmat,afterMaxpmat,(row-2));

	i=0;
	j=0;
	fd = open(outputName,O_WRONLY|O_CREAT|O_TRUNC,0644);
	writeBuf=(char*)malloc(sizeof(char)*4);
	if(fd<0){
		perror("open error");
		exit(999);
	}else{
		while(1){
			if(i==(row-2)/2&&j==0){
				write(fd,"\n",1);
				break;
			}
			if(j!=(row-2)/2){
				sprintf(writeBuf,"%d",afterMaxpmat[i][j]);
				if(afterMaxpmat[i][j]>=100){
					write(fd," ",1);
					write(fd,writeBuf,3);
				}else if(afterMaxpmat[i][j]>=10){
					write(fd," ",1);
					write(fd," ",1);
					write(fd,writeBuf,2);
				}else if(afterMaxpmat[i][j]>=0){
					write(fd," ",1);
					write(fd," ",1);
					write(fd," ",1);
					write(fd,writeBuf,1);
				}else if(afterMaxpmat[i][j]>=-9){
					write(fd," ",1);
					write(fd," ",1);
					write(fd,writeBuf,2);
				}else if(afterMaxpmat[i][j]>=-99){
					write(fd," ",1);
					write(fd,writeBuf,3);
				} else if(afterMaxpmat[i][j]>=-999){
					write(fd,writeBuf,4);
				}

				if(j==((row-2)/2)-1){
					write(fd,"\n",1);
				}else{
					write(fd," ",1);
				}

				j++;
			}else{
					i++;
					j=0;
			}
		}
	}

	close(fd);

	free(writeBuf);

	freeAllocation(layerMatrix,row);
	freeAllocation(filter,3);
	freeAllocation(afterConvmat,row-2);
	freeAllocation(afterMaxpmat,(row-2)/2);

	return 0;
}

void conv(int** matrix, int** filter,int** aftermat,int row){
	int i,j;
	int count=0;
	int countterm=0;
	int check_thread;
	int check_join;
	pthread_t *threadId;
	MatrixData *matData;
	void *t_return;

	threadId =(pthread_t*)malloc(sizeof(pthread_t)*(row-2)*(row-2));
	matData = (MatrixData*)malloc(sizeof(MatrixData));
	matData->mat=matrix;
	matData->filter=filter;

	for(i=0 ;i < row-2;i++){
		for(j=0 ;j<row-2;j++){
			matData = (MatrixData*)malloc(sizeof(MatrixData));
			matData->mat=matrix;
			matData->filter=filter;
			matData->i=i;
			matData->j=j;
			check_thread = pthread_create(&threadId[count],NULL,convCal, matData);
			if(check_thread<0){
				perror("pthread_create");
				exit(999);
			}
			count++;
			countterm++;

			if(countterm==10 || count==(row-2)*(row-2)){
				for(countterm;countterm>0;countterm--){
					check_join=pthread_join(threadId[count-countterm],&t_return);
					if(check_join<0){
						perror("join error");
						exit(999);
					}
					aftermat[(count-countterm)/(row-2)][(count-countterm)%(row-2)]=t_return;
				}
			} 

		}
	}

};	

void *convCal(void *temp){

	MatrixData matD = *((MatrixData*)temp);
	int sum =0;
	int x,y;
	int f_x=0,f_y;
	for(x=matD.i;x<matD.i+3;x++,f_x++){
		f_y=0;
		for(y=matD.j;y<matD.j+3;y++,f_y++){
			sum+= matD.mat[x][y]*matD.filter[f_x][f_y];
		}
	}
	
	return (void*)sum;
};

void maxPooling(int** matrix,int** aftermat,int row){

	int i,j;
	int count=0;
	int countterm=0;
	int check_thread;
	int check_join;
	pthread_t *threadId;
	MatrixData *matData;
	void *t_return;

	threadId =(pthread_t*)malloc(sizeof(pthread_t)*(row/2)*(row/2));

	for(i=0 ;i < row;){
		for(j=0 ;j<row;){
			matData = (MatrixData*)malloc(sizeof(MatrixData));
			matData->mat=matrix;
			matData->i=i;
			matData->j=j;
			check_thread = pthread_create(&threadId[count],NULL,poolCal, matData);
			if(check_thread<0){
				perror("pthread_create");
				exit(999);
			}
			j+=2;
			count++;
			countterm++;

			if(countterm==10 || count==(row/2)*(row/2)){
				for(countterm;countterm>0;countterm--){
					check_join=pthread_join(threadId[count-countterm],&t_return);
					if(check_join<0){
						perror("join error");
						exit(1);
					}
					aftermat[(count-countterm)/(row/2)][(count-countterm)%(row/2)]=t_return;
				}
			} 

		}
		i +=2;
	}

};

void *poolCal(void *temp){
	
	MatrixData matD = *((MatrixData*)temp);
	int max =matD.mat[matD.i][matD.j];
	int x,y;
	for(x=matD.i;x<matD.i+2;x++){
		for(y=matD.j;y<matD.j+2;y++){
			if(max<matD.mat[x][y]){
				max=matD.mat[x][y];
			}
		}
	}

	return (void*)max;
	
};

int** makeAllocation(int** matrix,int row){

	int i=0;
	matrix=(int**)malloc(sizeof(int*)*row);
	for(i;i<row;i++){
		matrix[i]=(int*)malloc(sizeof(int)*row);
	}

	return matrix;
};

void freeAllocation(int** matrix, int row){

	for(int i=0;i<row;i++){
		free(matrix[i]);
	}
	free(matrix);
};

