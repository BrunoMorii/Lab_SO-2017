/*
 * RSFS - Really Simple File System
 *
 * Copyright © 2010 Gustavo Maciel Dias Vieira
 * Copyright © 2010 Rodrigo Rocco Barbieri
 *
 * This file is part of RSFS.
 *
 * RSFS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define CLUSTERSIZE 4096

unsigned short fat[65536];
unsigned short form; //flag que define se disco esta formatado corretamente
typedef struct {
       char used;
       char name[25];
       unsigned short first_block;
       int size;
} dir_entry;

dir_entry dir[128];

typedef struct id_file { //struct para auxiliar manipulaçao de arquivos
  int estado; //modo de abertura
  int bloco; //bloco atual do arquivo sendo lido
  int posicao; //posicao atual sendo lida no bloco atual
  char buffer[4096]; //dados do bloco
}id_file;

id_file open_files[128]; //vetor de estruturas para manipulaç~ao dos arquivos

int fs_save(){
  int i;

  //salva a fat em disco
  for(i = 0; i < 8 * 32; i++) {
    bl_write(i, (char *)&fat[(i*SECTORSIZE/sizeof(unsigned short))]);
  }

  //salva diretorio em disco
  for(i = 0; i < 8; i++){
    bl_write(256 + i, (char *)&dir[i * SECTORSIZE/sizeof(dir_entry)]);
  }

  return 1;
}

int fs_init() {
	int i = 0;
	form = 1;

  //recebe a fat do disco
	for(i = 0; i < 8 * 32; i++) {
		if(!bl_read(i, (char *)&fat[(i*SECTORSIZE/sizeof(unsigned short))]))
      return 0;
	}

  //verifica formataçao do disco
 	for(i = 0; i <= 32; i++) {
 		if(i < 32 && fat[i] != 3) {
 			puts("Disco não formatado!");
 			form = 0;
 			break;
 		} else if (i == 32 && fat[i] != 4) {
 			puts("Disco não formatado!");
 			form = 0;
 		}
 	}

  //declara abertura de todos arquivos como fechados
  for(i = 0; i < 128; i++){
    open_files[i].estado = -1;
    open_files[i].bloco = -1;
    open_files[i].posicao = -1;
  }

  //se formatado recebe diretorio em disco
  if(form){
    for(i = 0; i < 8; i++){
      //dir[i] = ((dir_entry *)buffer)[i];
      bl_read(256 + i, (char *)&dir[i * SECTORSIZE/sizeof(dir_entry)]);
    }
  }

	return 1;
}

int fs_format() {
	int i;
  //formata fat e diretorio
	for(i = 0; i < 65536; i++) {
 		if(i < 32) {
 			fat[i] = 3;
 		} else if (i == 32) {
 			fat[i] = 4;
 		} else {
 			fat[i] = 1;
 		}
    if(i < 128){
      dir[i].used = 'N';  //inicializando o diretório
    }
 	}

 	form = 1; //da como formatado
  fs_save(); //salva alteraçoes de formatacao
	return 1;
}

int fs_free() {
  int i, cont = 0;

  //verifica quantos blocos estao livres
  for(i = 33; i < bl_size()/8; i++) {
    if(fat[i] == 1) {
      cont++;
    }
  }

  //retorna bytes livres
  return cont * CLUSTERSIZE;
}

int fs_list(char *buffer, int size) {
  if(!form){
  	puts("Disco não formatado!");
  	return 0;
  }

  int i;
  char *aux = buffer;
  buffer[0] = '\0'; //inciializa buffer

  //para toda entrada do diretorio, verifica o arquivo
  for(i = 0; i < 128; i++) {
    if(dir[i].used == 'S'){ //se for entrada valida copia na formataçao correta para o buffer
      aux += sprintf(aux, "%s\t\t%d\n", dir[i].name, dir[i].size);
    }
  }

  //condiçao para informar direotorio vazio
  if(buffer[0] == '\0'){
    printf("Diretório Vazio!");
  }
  return 1;
}

int fs_create(char* file_name) {
  if(!form){
  	puts("Disco não formatado!");
  	return 0;
  }

  int i, flag = -1;

  if(strlen(file_name) > 24){ //verificar tamnho do nome do arquivo
    puts("Nome de arquivo maior que o permitido (24 caracteres)");
    return 0;
  }

  for(i = 0; i < 128; i++){
    if(dir[i].used == 'S' && strcmp(dir[i].name, file_name) == 0){ //Verificar se já existe arquivo com esse nome
      printf("Arquivo com %s já existe\n", file_name);
      return 0;
    }else if(dir[i].used == 'N' && flag == -1){ //verifica primeiro espaço livre no diretório
      flag = i;
    }
  }

  if(flag == -1){ //Se não houver espaço livre no disco
    puts("Disco cheio!");
    return 0;
  }

  //coloca arquivo no diretorio e da entrada como usada
  dir[flag].used = 'S';
  strcpy(dir[flag].name, file_name);
  dir[flag].size = 0;

  //busca qual bloco na fat sera usado
  for(i = 33; i < bl_size()/8; i++) {
    if(fat[i] == 1) {
      fat[i] = 2;
      break;
    }
  }

  //define o bloco da fat como primeiro
  dir[flag].first_block = i;
  fs_save(); //salva alteraçoes
  return 1;
}

int fs_remove(char *file_name) {
  if(!form){
  	puts("Disco não formatado!");
  	return 0;
  }

  int i, flag = -1;

  if(strlen(file_name) > 24){ //verificar tamnho do nome do arquivo
    puts("Arquivo inexistente!");
    return 0;
  }

  for(i = 0; i < 128; i++){ //busca o arquivo a ser removido pelos diretorios
    if(dir[i].used == 'S' && strcmp(dir[i].name, file_name) == 0){
      flag = i;
      break;
    }
  }

  if(flag == -1){ //se nao achar o arquivo
    puts("Arquivo inexistente!");
    return 0;
  }

  //removendo arquivo do diretorio
  dir[flag].used = 'N';
  dir[flag].size = 0;
  i = dir[flag].first_block; //recebe primeiro bloco para percorrer a fat

  //percorre a fat em busca dos blocos do arquivo removido
  while(fat[i] != 2){
    flag = fat[i];
    fat[i] = 1;
    i = flag;
  }

  //para o ultimo bloco 
  fat[i] = 1;
  fs_save(); //salva as alteraçoes
  return 1;
}

int fs_open(char *file_name, int mode) {
  if(!form){
  	puts("Disco não formatado!");
  	return -1;
  }

  int i, flag = -1;

  for(i = 0; i < 128; i++){
    if(dir[i].used == 'S' && strcmp(dir[i].name, file_name) == 0){ //Verificar se já existe arquivo com esse nome
      //caso seja pra leitura nao e necessaria essa mensagem
      if(mode != FS_R){
        printf("Arquivo com %s já existe!\n Recriando...\n", file_name);
      }

      flag = i;
      break;
    }else if(dir[i].used == 'N' && flag == -1 && mode == FS_W){ //verifica primeiro espaço livre no diretório
      flag = i;
    }
  }

  if(flag == -1 && mode == FS_W){ //Se não houver espaço livre no disco e for escrever
    puts("Disco cheio!\n");
    return -1;
  }

  if(flag == -1 && mode == FS_R){ //se o diretorio nao e usado e e modo de leitura
    printf("Arquivo para leitura inexistente!\nNome: %s\n", file_name);
    return -1;
  }

  if(mode == FS_W){
    //remove o arquivo se usado
    if(dir[flag].used == 'S'){
      fs_remove(file_name);
    }
   
    //cria entrada para ele em um diretorio
    fs_create(file_name);
  }
  int j;
  //busca onde foi criado e abre para leitura ou escrita
  for(i = 0; i < 128; i++){
    if(dir[i].used == 'S' && strcmp(dir[i].name, file_name) == 0){ //Verificar se já existe arquivo com esse nome
      open_files[i].estado = mode;
      open_files[i].bloco = dir[i].first_block;
      open_files[i].posicao = 0;
      for(j = 0; j < 8; j++){
      	bl_read((open_files[i].bloco * 8)+ j, &open_files[i].buffer[j * SECTORSIZE]);
      }
      return i;
    }
  }
    return -1;
}

int fs_close(int file)  {
  if(!form){
  	puts("Disco não formatado!");
  	return 0;
  }

  if(dir[file].used == 'N'){
    printf("Arquivo inexistente!\n");
    return 0;
  }

  if(open_files[file].estado == -1){
    printf("Arquivo não está aberto!\n");
    return 0;
  }

  int i;

  //escreve no disco o bloco atual em ram
  for(i = 0; i < 8; i++) {
    bl_write(open_files[file].bloco * 8 + i, &open_files[file].buffer[i * SECTORSIZE]);
  }

  //fecha na struct de informaçoes
  open_files[file].estado = -1;
  open_files[file].bloco = -1;
  open_files[file].posicao = -1;

  //salva as alterações no diretorio
  fs_save();

  return 1;
}

int fs_fatFree(){
  //funcao para buscar bloco livre
  int i;
  for(i = 33; i < bl_size()/8; i++){
    if(fat[i] == 1){
      return i;
    }
  }
  return -1;
}

int fs_write(char *buffer, int size, int file) {
  /* se o disco não estiver formatado */
  if(!form){
  	puts("Disco não formatado!");
  	return -1;
  }

  /* se o arquivo não estiver aberto */
  if(open_files[file].estado == -1){
    printf("Arquivo não aberto!\n");
    return -1;
  }

  /* se o arquivo não estiver aberto para escrita */
  if(open_files[file].estado != FS_W){
    printf("Arquivo não aberto para modo de escrita!\n");
    return -1;
  }

  int i, resto, prox_bloco;

  if(open_files[file].posicao + size > CLUSTERSIZE){ //caso queira escrever algo que ultrapasse um bloco
    resto = CLUSTERSIZE - open_files[file].posicao;
    strncat(open_files[file].buffer, buffer, resto); //copia a quantidade que falta para completar um bloco

    /* escreve bloco completo no disco */
  	for(i = 0; i < 8; i++){
      bl_write(open_files[file].bloco * 8 + i, &open_files[file].buffer[i * SECTORSIZE]);
    }

    /* acha o próximo bloco livre na FAT */
    prox_bloco = fs_fatFree();

    if(prox_bloco == -1){ //se o disco estiver cheio
      printf("Disco Cheio, não foi possível escrever os %d bytes do Arquivo\n", size);
      printf("Foram escritos apenas %d bytes\n", resto);
      dir[file].size += resto;
      return 0;
    }else{ //caso haja espaço no disco

      fat[open_files[file].bloco] = prox_bloco; //mapeia na FAT mais um bloco para arquivo
      fat[prox_bloco] = 2; //indica fim de arquivo para FAT
      open_files[file].bloco = prox_bloco; //"ponteiro" do arquivo aberto para o novo bloco

      open_files[file].buffer[0] = '\0';
      strncat(open_files[file].buffer, &buffer[resto], size - resto); //concatena no buffer do arquivo aberto o que ainda não foi escrito
      open_files[file].posicao = size - resto; //atualiza posição do arquivo aberto dentro do bloco
    }
  }else{ //se quiser escrever algo que caiba no bloco atual
  	strncpy(&open_files[file].buffer[open_files[file].posicao], buffer, size); //concatena no buffer do arquivo aberto o buffer solicitado
  	open_files[file].posicao += size; //atualiza posição do arquivo aberto dentro do bloco
    open_files[file].buffer[open_files[file].posicao] = '\0';
  } 

  dir[file].size += size; //incrementa tamanho do arquivo

  return size; //todos os bytes solicitados foram escritos
}

int fs_read(char *buffer, int size, int file) {
  /* se o disco não estiver formatado */
  if(!form){
    puts("Disco não formatado!");
    return -1;
  }

  /* se o arquivo não estiver aberto */
  if(open_files[file].estado == -1){
    printf("Arquivo não aberto!\n");
    return -1;
  }

  /* se o arquivo não estiver aberto para leitura */
  if(open_files[file].estado != FS_R){
    printf("Arquivo não aberto para modo de leitura!\n");
    return -1;
  }

  int i, resto, size_read;

  //se o arquivo for completamente lido
  if(fat[open_files[file].bloco] == 2 && (open_files[file].posicao == dir[file].size % 4096)){
  	return 0;
  }

  // Valor restante a ser lido no último bloco
  if(fat[open_files[file].bloco] == 2) {
  	size_read = (dir[file].size % 4096) - open_files[file].posicao;
  	//ajusta valores
  	if(size_read > size) {
  		size_read = size;
  	}
  } else {
  	size_read = size;
  }

  if(open_files[file].posicao + size_read > CLUSTERSIZE){ //caso queira ler algo que ultrapasse o bloco atual
    resto = CLUSTERSIZE - open_files[file].posicao;
    strncpy(buffer, &(open_files[file].buffer[open_files[file].posicao]), resto); //passa para o buffer o que falta daquele bloco
    buffer[resto] = '\0';

    /* acha o próximo bloco do arquivo */
    open_files[file].bloco = fat[open_files[file].bloco];; //"ponteiro" do arquivo aberto para o novo bloco
    if (open_files[file].bloco == 2){ //fim de arquivo, não tem mais o que ler
        printf("Não foi possível ler os %d bytes do Arquivo\n", size_read);
        printf("Foram lidos apenas %d bytes\n", resto);
        return resto;
    }

    /* lê bloco completo no disco */
    for(i = 0; i < 8; i++){
      bl_read(open_files[file].bloco * 8 + i, &(open_files[file].buffer[i * SECTORSIZE])); //passa para o buffer o novo bloco lido
    }

      strncat(buffer, open_files[file].buffer, size_read - resto); //concatena no buffer do arquivo aberto o que ainda não foi escrito
      open_files[file].posicao = size_read - resto;

  }else{ //caso tudo o que queira ler esteja no bloco atual

      strncpy(buffer, &(open_files[file].buffer[open_files[file].posicao]), size_read); //copia para o buffer o que ainda não foi escrito
      buffer[size_read] = '\0';
      open_files[file].posicao += size_read; //atualiza posição do arquivo aberto dentro do bloco
  }

  return size_read;
}  


