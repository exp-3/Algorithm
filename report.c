#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>


typedef struct {
  char *beginPtr;
  char *endPtr;
  int count;
} word;

#define HASH_SIZE 1048576

word wordsInFirstHalf[HASH_SIZE];
word wordsInSecondHalf[HASH_SIZE];

word *sortedWordPtrsInFirstHalf[5];
word *sortedWordPtrsInSecondHalf[5];

int HASH_CONFLICT_COUNT = 0;
int WORD_NUM = 0;
int MAX_CONFLICT_COUNT = 0;

char *countWordInFirstHalf(char *start); //前半部分をカウント、後半の最初のアドレスを返す
char *countWordInSecondHalf(char *start); //後半部分をカウント

void sortWords(word *words, word **result, int size);

void printMostFiveWords(word **sortedWordPtrs) {
  int i, j;
  for(i = 0; i < 5; i++) {
    for(j = 0; j < sortedWordPtrs[i]->endPtr - sortedWordPtrs[i]->beginPtr; j++) {
      putchar(sortedWordPtrs[i]->beginPtr[j]);
    }
    putchar('\n');
  }
}

//                        d8b
//                        Y8P
// 88888b.d88b.   8888b.  888 88888b.
// 888 "888 "88b     "88b 888 888 "88b
// 888  888  888 .d888888 888 888  888
// 888  888  888 888  888 888 888  888
// 888  888  888 "Y888888 888 888  888
int main() {
  struct timeval precount;
  struct timeval postcount;
  struct timeval postsort;

  int i;
  for(i = 0; i < HASH_SIZE; i++) {
    wordsInFirstHalf[i].beginPtr = NULL;
    wordsInFirstHalf[i].endPtr = NULL;
    wordsInSecondHalf[i].beginPtr = NULL;
    wordsInSecondHalf[i].endPtr = NULL;
  }
  int filesize, pagesize, mmapsize;
  char *addr;

  filesize = lseek(0, 0, SEEK_END);
  pagesize = getpagesize();
  mmapsize = (filesize + (pagesize - 1)) / pagesize * pagesize;
  addr = (char *)mmap(0, mmapsize, PROT_READ, MAP_PRIVATE, 0, 0);

  gettimeofday(&precount, NULL);
  countWordInSecondHalf(countWordInFirstHalf(addr));

  gettimeofday(&postcount, NULL);

  sortWords(wordsInFirstHalf, sortedWordPtrsInFirstHalf, HASH_SIZE);
  sortWords(wordsInSecondHalf, sortedWordPtrsInSecondHalf, HASH_SIZE);

  gettimeofday(&postsort, NULL);
  printMostFiveWords(sortedWordPtrsInFirstHalf);
  printMostFiveWords(sortedWordPtrsInSecondHalf);

  printf("\n==========================\n");
  printf("hash conflict occurred %d times.\n", HASH_CONFLICT_COUNT);
  printf("word scanned totally %d times.\n", WORD_NUM);
  printf("average conflict count is %lf.\n", (double)HASH_CONFLICT_COUNT / WORD_NUM);
  printf("max conflict count is %d.\n", MAX_CONFLICT_COUNT);
  printf("count spend %ld useconds.\n", postcount.tv_usec - precount.tv_usec);
  printf("sort spend %ld useconds.\n", postsort.tv_usec - postcount.tv_usec);

  return 0;
}

//          888    d8b 888
//          888    Y8P 888
//          888        888
// 888  888 888888 888 888
// 888  888 888    888 888
// 888  888 888    888 888
// Y88b 888 Y88b.  888 888
//  "Y88888  "Y888 888 888
int hash(char *begin, char *end) {
    long h = 0;
    int L = end - begin;

    int i;
    for (i = 0; i < L; i++) {
      char c = begin[i];
      c |= 0x20; //'a' - 'A' = 0x20なので、強制的に大文字を小文字に変換
      h = h * 37 + c;
    }
    return abs(h) % HASH_SIZE;
}

int isAlphabet(char c) {
  c |= 0x20; // 'a' - 'A' = 0x20なので、強制的に大文字を小文字に変換
  return (c >= 'a' && c < 'a' + 26);
}

int isValidCharactor(char c) {
  return isAlphabet(c) || c == '\'' || c == '-' || (c >= '0' && c <= '9');
}

void parseWord(char *start, char **end) { //startから続く単語の終わりを探し、その次のアドレスを格納して返す
  *end = start + 1;

  //最後の文字を探す
  while(1) {
    if(!isValidCharactor(**end)) {
      if(*(*end - 1) == '\'' || *(*end - 1) == '-') {
        (*end)--;
        return;
      }
      return;
    }

    (*end)++;
  }
}

int isTheSameWord(char *begin1, char *end1, char *begin2, char *end2) {
  int i;
  for(i = 0; i < end1 - begin1; i++) {
    if((begin1[i] ^ begin2[i]) & ~0x20) { // もし6bit目以外で違いが存在したら
      return 0;
    }
  }

  return 1;
}

//  .d888 d8b                  888         888               888  .d888
// d88P"  Y8P                  888         888               888 d88P"
// 888                         888         888               888 888
// 888888 888 888d888 .d8888b  888888      88888b.   8888b.  888 888888
// 888    888 888P"   88K      888         888 "88b     "88b 888 888
// 888    888 888     "Y8888b. 888         888  888 .d888888 888 888
// 888    888 888          X88 Y88b.       888  888 888  888 888 888
// 888    888 888      88888P'  "Y888      888  888 "Y888888 888 888
void registerWordInFirstHalf(char *begin, char *end) {
  int h = hash(begin, end);
  int conflictCount = 0;
  while(1) {
    if(wordsInFirstHalf[h].beginPtr == NULL && wordsInFirstHalf[h].endPtr == NULL) {
      wordsInFirstHalf[h].beginPtr = begin;
      wordsInFirstHalf[h].endPtr = end;
      wordsInFirstHalf[h].count = 1;
      if(MAX_CONFLICT_COUNT < conflictCount) {
        MAX_CONFLICT_COUNT = conflictCount;
      }
      return;
    }

    if(isTheSameWord(wordsInFirstHalf[h].beginPtr, wordsInFirstHalf[h].endPtr,
                     begin, end)) {
      wordsInFirstHalf[h].count++;
      if(MAX_CONFLICT_COUNT < conflictCount) {
        MAX_CONFLICT_COUNT = conflictCount;
      }
      return;
    }

    conflictCount++;
    HASH_CONFLICT_COUNT++;
    h++;
    h %= HASH_SIZE; // もしhashのサイズを超えたら
  }
}

char *countWordInFirstHalf(char *start) {
  char *begin, *end;
  begin = start;

  while(1) {
    if(isAlphabet(*begin)) {
      WORD_NUM++;
      parseWord(begin, &end);
      registerWordInFirstHalf(begin, end);
      begin = end;
      continue;
    }

    if(*begin == '@') {
      return begin++;
    }

    begin++;
  }
}

//                                                  888      888               888  .d888
//                                                  888      888               888 d88P"
//                                                  888      888               888 888
// .d8888b   .d88b.   .d8888b .d88b.  88888b.   .d88888      88888b.   8888b.  888 888888
// 88K      d8P  Y8b d88P"   d88""88b 888 "88b d88" 888      888 "88b     "88b 888 888
// "Y8888b. 88888888 888     888  888 888  888 888  888      888  888 .d888888 888 888
//      X88 Y8b.     Y88b.   Y88..88P 888  888 Y88b 888      888  888 888  888 888 888
//  88888P'  "Y8888   "Y8888P "Y88P"  888  888  "Y88888      888  888 "Y888888 888 888
void registerWordInSecondHalf(char *begin, char *end) {
  int conflictCount = 0;
  int h_firstHalf, h_secondHalf;
  h_firstHalf = h_secondHalf = hash(begin, end);
  while(1) {
    //もし前半の語群に存在しなかったら
    if(wordsInFirstHalf[h_firstHalf].beginPtr == NULL &&
       wordsInFirstHalf[h_firstHalf].endPtr == NULL) {
      while(1) {
        //もし後半の語群にも初出であれば
        if(wordsInSecondHalf[h_secondHalf].beginPtr == NULL &&
           wordsInSecondHalf[h_secondHalf].endPtr == NULL) {
          wordsInSecondHalf[h_secondHalf].beginPtr = begin;
          wordsInSecondHalf[h_secondHalf].endPtr = end;
          wordsInSecondHalf[h_secondHalf].count = 1;

          if(MAX_CONFLICT_COUNT < conflictCount) {
            MAX_CONFLICT_COUNT = conflictCount;
          }
          return;
        }

        //後半の語群に既出であれば
        if(isTheSameWord(wordsInSecondHalf[h_secondHalf].beginPtr,
                         wordsInSecondHalf[h_secondHalf].endPtr,
                         begin, end)) {
          wordsInSecondHalf[h_secondHalf].count++;

          if(MAX_CONFLICT_COUNT < conflictCount) {
            MAX_CONFLICT_COUNT = conflictCount;
          }
          return;
        }

        HASH_CONFLICT_COUNT++;
        conflictCount++;
        h_secondHalf++;
        h_secondHalf %= HASH_SIZE; // もしhashのサイズを超えたら
      }
    }

    //もし前半の語群に存在したら
    if(isTheSameWord(wordsInFirstHalf[h_firstHalf].beginPtr,
                     wordsInFirstHalf[h_firstHalf].endPtr,
                     begin, end)) {
      wordsInFirstHalf[h_firstHalf].count = 0;

      if(MAX_CONFLICT_COUNT < conflictCount) {
        MAX_CONFLICT_COUNT = conflictCount;
      }
      return;
    }

    HASH_CONFLICT_COUNT++;
    conflictCount++;
    h_firstHalf++;
    h_firstHalf %= HASH_SIZE; // もしhashのサイズを超えたら
  }
}

char *countWordInSecondHalf(char *start) {
  char *begin, *end;
  begin = start;

  while(1) {
    if(isAlphabet(*begin)) {
      WORD_NUM++;
      parseWord(begin , &end);
      registerWordInSecondHalf(begin, end);
      begin = end;
      continue;
    }

    if ((int)(*begin) == EOF){
      return begin;
    }

    begin++;
  }
}

//                           888
//                           888
//                           888
// .d8888b   .d88b.  888d888 888888
// 88K      d88""88b 888P"   888
// "Y8888b. 888  888 888     888
//      X88 Y88..88P 888     Y88b.
//  88888P'  "Y88P"  888      "Y888

void sortWords(word *words, word **result, int size) {
  int min = 0; // 暫定5位の出現数
  int num = 0; // 要素数. nullポインタのメンバ変数を参照しようとするのを避けるため.
  int i, j, index;
  for(i = 0; i < size; i++) {
    if(words[i].count > min) {
      index = 0;
      // words[i]の出現数が暫定何番目となるのかを0番目の要素から順に比較して調べる。
      while(index < num && words[i].count <= result[index]->count) {
        // index < num としているのは、まだresultに4個以下の要素しか登録されていない場合に
        // result[num以上の値]->countとしてしまうと、null->countとなってしまい、
        // nullへの参照が起きてエラーとなることを避けるため。
        // words[i]の出現数の方が大きくなったら抜ける
        index++;
      }

      // 末尾の要素から順に順位を一つ下にずらしていく
      for(j = 4; j > index; j--) {
        result[j] = result[j - 1];
      }

      // index番目の後釜にwords[i]を据える
      result[index] = &words[i];

      // resultがもしまだ5個要素を持っていなかったら、要素数を増やす
      if(num < 5) {
        num++;
      }

      // 暫定5位の出現数を更新する
      min = result[num - 1]->count;
    }
  }
}
