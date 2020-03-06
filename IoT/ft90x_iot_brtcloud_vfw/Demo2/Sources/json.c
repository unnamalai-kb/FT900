#include "json.h"
#include <stdlib.h>
#include "iot_demos.h"
// returns true if X and Y are same
static int compare(const char *X, const char *Y)
{
     while (*X && *Y)
     {
          if (*X != *Y)
               return 0;

          X++;
          Y++;
     }

     return (*Y == '\0');
}

// Function to implement strstr() function
static char* c_strstr(char* X, const char* Y)
{
     int object_count = 0;
     //printf("[%s] %s \n\n", Y, X);
     while (*X != '\0')
     {
          switch(*X)
          {
          case '{':
               object_count++;
               break;

          case '}':
               object_count--;
               break;

          }

          if (object_count == 1)
               if ((*X == *Y) && compare(X, Y))
                    return X;
          X++;
     }

     return NULL;
}

uint32_t json_parse_int( const char* ptr, char* key )
{
     char* start = NULL;
     char* stop = NULL;

     // the key is always followed by 3 characters "\": "
     start = strstr((char*)ptr, key);
     if (!start) {
          return -2; // key is not found
     }
     start += strlen(key) + 3;

     // the terminator of the key value is either a comma or a close parenthesis (if last parameter)
     stop = strchr(start, ',');
     if (!stop) {
          stop = strchr(start, '}');
          if (!stop) {
               return -3; // terminator character not found
          }
     }

     return strtoul(start, &stop, 10);
}

#if 1
char* json_parse_str( const char* ptr, char* key, int* len )
{
     char* start = NULL;
     char* stop = NULL;

     // the key is always followed by 4 characters "\": \""
     start = strstr(ptr,key);//c_strstr((char*)ptr, key);

     if (!start) {
          return NULL; // key is not found
     }
     start += strlen(key) + 4;

     // the terminator of the key value is a "
     stop = strchr(start, '\"');
     if (!stop) {
          return NULL; // terminator character not found
     }

     if (len) {
          *len = stop-start;
     }
     return start;
}
#else
#endif

char* json_parse_str_ex(  char* ptr, char* key, char end )
{
     char* temp = NULL;

     temp = strchr(ptr, end);
     if (!temp) {
          return NULL;
     }
     ptr[temp-ptr] = '\0';
     ptr[temp-ptr-1] = '\0';

     temp = strstr(ptr, key);
     if (!temp) {
          return NULL;
     }
     ptr += (temp-ptr) + strlen(key);
     ptr ++;

     return ptr;
}

char* json_parse_object( const char* ptr, const char* key, int* len)
{
     char* start = NULL;
     char* stop = NULL;
     int object_count = 0;
     int start_len = 0;
     bool break_loop = false;
     bool key_found = false;
     char c = '\0', pc = '\0';
     // the key is always followed by 4 characters "\": \""
     start = strstr((char*)ptr, key);
     if (!start) {
          printf("strstr failed\n");
          return NULL; // key is not found
     }
     start += strlen(key);
     stop = start;

     while (stop != NULL)
     {
          c = *stop;
          //printf("%c",c);
          switch(c)
          {
          case '{':
               object_count++;
               if (object_count == 1)
                    start = stop;
               break;

          case '}':
               object_count--;
               if (object_count == 0)
                    break_loop = true;
               break;

          default:
               break;
          }

          if (break_loop)
               break;

          stop++;
     }
     // the terminator of the key value is a "
     if (!stop) {
          return NULL; // terminator character not found
     }

     if (len) {
          *len = stop-start;
     }

     return start;
}


char* json_parse_object_array( const char* ptr, const char* key, char* len, uint idx)
{
     char* start = NULL;
     char* stop = NULL;
     uint8_t object_count = 0;
     uint8_t  child_object_count = 0;
     bool break_loop = false;
     bool object_found = false;
     bool array_start = false;
     char c = '\0';
     // the key is always followed by 4 characters "\": \""
     start = strstr(ptr, key);

     if (!start) {
         return NULL; // key is not found
     }
     start += strlen(key);
     stop = start;

     while (stop != NULL)
     {
         c = *stop;
         //printf("%c",c);
         switch(c)
         {
         case '[':
             array_start = true;
             break;

         case '{':

             if(object_found)
             {
                 child_object_count++;
               //  DPRINTF_INFO(">> child_object_count %d",child_object_count);
             }else if (array_start)
             {
                 start = stop;
                 object_found = true;
             }

             break;

         case '}':
             if (object_found)
             {
                 if(child_object_count > 0){
                     child_object_count--;
                 }else{
                     object_count++;
                     if (object_count == idx)
                         break_loop = true;
                     else
                     {
                         object_found = false;
                     }
                 }
             //    DPRINTF_INFO("<< child_object_count %d",child_object_count);
             }
             else
             {
                 stop = NULL;
                 break_loop = true;
             }
             break;

         case ']':
             if (!array_start)
                 stop = NULL;

             break_loop = true;
             break;

         default:
             break;
         }

         if (break_loop)
             break;

         stop++;
     }
     // the terminator of the key value is a "
     if (!stop || !object_found) {
         return NULL; // terminator character not found
     }

     if (len) {
        *len = stop-start;
     }
     DPRINTF_INFO("%s",start);
     return start;
}

char* json_parse_object_multiarray( const char* ptr, const char* key, int* len, int arr_idx, int obj_idx)
{
    char* start = NULL;
    char* stop = NULL;
    int array_count = -1;
    int object_count = 0;
    int sub_object_count = 0;
    bool break_loop = false;
    bool object_found = false;
    bool array_start = false;
    char c = '\0';
    // the key is always followed by 4 characters "\": \""
    start = strstr(ptr, key);
    if (!start) {
        return NULL; // key is not found
    }
    start += strlen(key);
    stop = start;

    while (stop != NULL)
    {
        c = *stop;
        //printf("%c",c);
        switch(c)
        {
        case '[':
             array_count++;
             if (array_count == arr_idx)
             {
                  //printf("\narr_count %d\n", arr_idx);
                  array_start = true;
             }
            break;

        case '{':
            if (array_start)
            {
                sub_object_count++;
                //printf("\nsubobj++ %d\n", sub_object_count);
                if (sub_object_count == 1)
                    start = stop;
                object_found = true;
            }
            break;

        case '}':
            if (object_found)
            {
                sub_object_count--;
                //printf("\nsubobj-- %d\n", sub_object_count);
                if (sub_object_count == 0)
                {
                     object_count++;
                     //printf("\nobj++ %d vs %d\n", object_count, obj_idx);
                     if (object_count == obj_idx)
                          break_loop = true;
                     else
                     {
                          object_found = false;
                          sub_object_count = 0;
                     }
                }
            }
            else
            {
                 if (array_start)
                 {
                      //printf("1. NULL\n");
                      stop = NULL;
                      break_loop = true;
                 }
            }
            break;

        case ']':
             // It should be in the correct array
            if (array_start)
            {
                 //printf("Breaking\n");
                 break_loop = true;
            }
            else
            {
                 //printf("arrcount vs idx %d vs %d\n", array_count, arr_idx);
                 if (array_count > arr_idx)
                 {
                      //printf("2. NULL\n");
                      stop = NULL;
                      break_loop = true;
                 }
            }
            break;

        default:
            break;
        }

        if (break_loop)
            break;

        stop++;
    }
    // the terminator of the key value is a "
    if (!stop || !object_found) {
         //printf("3. NULL\n");
        return NULL; // terminator character not found
    }

    if (len) {
        *len = stop - start + 1;
        //printf("LEN %d\n", *len);
    }

    return start;
}
