#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define MAX_INODES 1024
#define NAME_LEN 32

typedef struct{
 uint32_t inode;
 uint32_t parent;
 char ftype;
 char name[NAME_LEN];
} entry_node;

//------------------------HELPER-FUNCTIONS------------------------------------------

//Loads inodes from the inodes_list file into the inodes array and error checks
int load_inodes(entry_node inodes[])
{
 FILE* fp = fopen("inodes_list", "rb");
 if(fp == NULL)
 {
  perror("fopen inodes_list");
  return -1;
 }

 //Reads one entry at a time, skips any with a bad inode number or file type
 entry_node entry;
 while(fread(&entry, sizeof(entry_node), 1, fp) == 1)
 {
  if(entry.inode >= MAX_INODES ||
     (entry.ftype != 'd' && entry.ftype != 'f'))
  {
   printf("Invalid inode entry %u ignored\n", entry.inode);
   continue;
  }

  inodes[entry.inode] = entry;
 }

 fclose(fp);
 return 0;
}

//Writes every inode that is in use back to the inodes_list file (overwrites it)
int save_inodes(entry_node inodes[])
{
 FILE *fp = fopen("inodes_list", "wb");
 if(fp == NULL)
 {
  perror("fopen inodes_list");
  return -1;
 }

 //Only writes the slots in use so the file stays a multiple of 44 bytes
 for (int i = 0; i < MAX_INODES; i++)
 {
  if(inodes[i].ftype != '\0')
  {
   if(fwrite(&inodes[i], sizeof(entry_node), 1, fp) != 1)
   {
    perror("fwrite inodes_list");
    fclose(fp);
    return -1;
   }
  }
 }

 fclose(fp);
 return 0;
}

//Checks if the character is a space or tab
int is_space(char c)
{
 return c == ' ' || c == '\t';
}

//Returns 0 if equal, negative if a < b, positive if a > b
int xstrcmp(const char *a, const char *b)
{
 while (*a != '\0' && *a == *b)
 {
  a++;
  b++;
 }
 return (unsigned char)*a - (unsigned char)*b;
}

//Checks if the stored name matches the given name (stored might not have a \0)
int name_matches(const char stored[], const char name[])
{
 for(int i = 0; i < NAME_LEN; i++)
 {
  if(stored[i] != name[i])
  {
   return 0;
  }

  if(stored[i] == '\0')
  {
   return 1;
  }
 }
 return 1;
}

//Returns the first unused inode number, or -1 if all 1024 are taken
int find_free_inode(entry_node inodes[])
{
 for (int i = 0; i < MAX_INODES; i++)
 {
  if(inodes[i].ftype == '\0')
  {
   return i;
  }
 }
 return -1;
}

//Returns 1 if the name already exists in the directory cwd, 0 if not
int name_exists(entry_node inodes[], uint32_t cwd, const char *name)
{
 for (int i = 0; i < MAX_INODES; i++)
 {
  if(inodes[i].ftype != '\0' && inodes[i].parent == cwd &&
     name_matches(inodes[i].name, name))
  {
   return 1;
  }
 }
 return 0;
}

//Lists all inodes in the current directory as long as they are in use and their parent is cwd
void do_ls(entry_node inodes[], uint32_t cwd)
{
 for (int i = 0; i < MAX_INODES; i++)
 {
  if(inodes[i].ftype != '\0' && inodes[i].parent == cwd)
  {
   printf("inode: %d, type: %c, name: %.32s\n",
          i, inodes[i].ftype, inodes[i].name);
  }
 }
}

//Moves into a directory inside the current one (cwd is a pointer so we can change it)
void do_cd(entry_node inodes[], uint32_t *cwd, const char *name)
{
 for (int i = 0; i < MAX_INODES; i++)
 {
  if(inodes[i].ftype == 'd' && inodes[i].parent == *cwd &&
     name_matches(inodes[i].name, name))
  {
   *cwd = i;
   return;
  }
 }
 printf("cd %s: not a directory or does not exist\n", name);
}

//Creates a new directory in the current directory and its real file with . and ..
void do_mkdir(entry_node inodes[], uint32_t cwd, const char *name)
{
 //Truncate to the first 32 chars
 char tname[NAME_LEN + 1];
 strncpy(tname, name, NAME_LEN);
 tname[NAME_LEN] = '\0';

 //Make sure the directory doesn't exist already
 if(name_exists(inodes, cwd, tname))
 {
  printf("mkdir: %s already exists\n", tname);
  return;
 }

 //Find a free inode if there is space
 int free_i = find_free_inode(inodes);
 if(free_i == -1)
 {
  printf("mkdir: no free inodes\n");
  return;
 }

 //Builds the real file's name (the inode number as text) then creates it
 char fname[16];
 snprintf(fname, sizeof(fname), "%d", free_i);
 FILE *fp = fopen(fname, "w");

 if(fp == NULL)
 {
  perror("mkdir: fopen");
  return;
 }

 fprintf(fp, "%d .\n%u ..\n", free_i, cwd);
 fclose(fp);

 //Fills in the new inode in the array (ftype is what marks it as in use)
 inodes[free_i].inode = free_i;
 inodes[free_i].parent = cwd;
 inodes[free_i].ftype = 'd';
 strncpy(inodes[free_i].name, tname, NAME_LEN);
}

//Creates a new file in the current directory (does nothing if the name exists)
void do_touch(entry_node inodes[], uint32_t cwd, const char *name)
{
 //Truncate to the first 32 chars
 char tname[NAME_LEN + 1];
 strncpy(tname, name, NAME_LEN);
 tname[NAME_LEN] = '\0';

 //If the file already exists in cwd do nothing
 if(name_exists(inodes, cwd, tname))
 {
  return;
 }

 //Find a free inode if there is space
 int free_i = find_free_inode(inodes);
 if(free_i == -1)
 {
  printf("touch: no free inodes\n");
  return;
 }

 //Builds the real file's name (the inode number as text) then creates it
 char fname[16];
 snprintf(fname, sizeof(fname), "%d", free_i);
 FILE* fp = fopen(fname, "w");

 if(fp == NULL)
 {
  perror("touch: fopen");
  return;
 }

 //The real file just holds the name of the file
 fprintf(fp, "%s\n", tname);
 fclose(fp);

 //Fills in the new inode in the array (ftype is what marks it as in use)
 inodes[free_i].inode = free_i;
 inodes[free_i].parent = cwd;
 inodes[free_i].ftype = 'f';
 strncpy(inodes[free_i].name, tname, NAME_LEN);
}
//-----------------------------------HELPER FUNCTIONS------------------------------------------


int main(int argc, char* argv[])
{
 //Takes in 1 argument after the file
 if(argc != 2)
 {
  printf("Need exaclty 1 argument\n", argv[0]);
  return 1;
 }

 //Checks to make sure the directory for the argument is valid and if so changes program into that directory
 if(chdir(argv[1]) != 0)
 {
  perror("chdir");
  return 1;
 }

 //Create an array of 1024 inodes and set every byte to 0 (nothing inside, padding too)
 entry_node inodes[MAX_INODES];
 unsigned char *bytes = (unsigned char *)inodes;
 for (size_t i = 0; i < sizeof(inodes); i++)
 {
  bytes[i] = 0;
 }

 if(load_inodes(inodes) != 0)
 {
  return 1;
 }

 //Inode 0 is the root so it has to be a directory
 if(inodes[0].ftype != 'd')
 {
  printf("Root inode 0 is not a directory\n");
  return 1;
 }

 char line[256];
 uint32_t currwd = 0;

 printf("> ");

 //Reads one line from keyboard(stdin) and end when (\n,crtlD) and auto append \0
 while(fgets(line, sizeof(line), stdin) != NULL)
 {
  //Finds the newline character in the input line
  //strchr: Returns a pointer to the first occurrence of the character, or NULL if not found
  char *nl = strchr(line, '\n');

  //Check if the newline character was found then replace it with a null terminator
  if(nl != NULL)
  {
   *nl = '\0';
  }

  //--- Moves past leading whitespace, cmd points to the start of the command
  char *cmd = line;
  while(is_space(*cmd))
  {
   cmd++;
  }
  //---

  //--- Finds the end of the command either a \0 or a whitespace
  char *arg = cmd;
  while(*arg != '\0' && !is_space(*arg))
  {
   arg++;
  }
  //---

  //--- First whitespace after end of cmd, replaces it with a null terminator and moves past it
  if(*arg != '\0')
  {
   *arg = '\0';
   arg++;
  }
  //---

  //--- Moves past whitespace after the command
  while(is_space(*arg))
  {
   arg++;
  }
  //---

  //--- Finds the end of the argument
  char *end = arg;
  while(*end != '\0' && !is_space(*end))
  {
   end++;
  }
  //---

  //--- Moves past whitespace after the argument
  char *rest = end;
  while(is_space(*rest))
  {
   rest++;
  }
  //---

  //--- If there are extra characters after the argument, mark the command as having too many arguments
  int too_many_args = (*rest != '\0');
  *end = '\0';
  //---

  if(*cmd == '\0')
  {
   //blank line: do nothing
  }
  else if(xstrcmp(cmd, "exit") == 0)
  {
   //exit: Only 1 cmd no args
   if(too_many_args || *arg != '\0')
   {
    printf("exit takes no arguments\n");
   }
   else
   {
    break;
   }
  }
  else if(xstrcmp(cmd, "ls") == 0)
  {
   //ls: Only 1 cmd no args
   if(too_many_args || *arg != '\0')
   {
    printf("ls takes no arguments\n");
   }
   else
   {
    do_ls(inodes, currwd);
   }
  }
  else if(xstrcmp(cmd, "cd") == 0)
  {
   //cd, mkdir, touch: Only 1 cmd 1 arg
   if(too_many_args || *arg == '\0')
   {
    printf("cd needs exactly one name\n");
   }
   else
   {
    do_cd(inodes, &currwd, arg);
   }
  }
  else if(xstrcmp(cmd, "mkdir") == 0)
  {
   if(too_many_args || *arg == '\0')
   {
    printf("mkdir needs exactly one name\n");
   }
   else
   {
    do_mkdir(inodes, currwd, arg);
   }
  }
  else if(xstrcmp(cmd, "touch") == 0)
  {
   if(too_many_args || *arg == '\0')
   {
    printf("touch needs exactly one name\n");
   }
   else
   {
    do_touch(inodes, currwd, arg);
   }
  }
  else
  {
   printf("Unknown command: %s\n", cmd);
  }

  printf("> ");
 }

 //Both exit and crtlD end up here so both save the inode list
 if(save_inodes(inodes) != 0)
 {
  return 1;
 }

 return 0;
}
