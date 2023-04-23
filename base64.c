#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>



/*
** Translation Table as described in RFC1113
*/
static const char cb64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


/*
** Translation Table to decode (created by author)
*/
static const char cd64[] = "|$$$}rstuvwxyz{$$$$$$$>?@ABCDEFGHIJKLMNOPQRSTUVW$$$$$$XYZ[\\]^_`abcdefghijklmnopq";


typedef enum {
    SYNTAX_ERROR = 1,
    FILE_ERROR,
    FILE_IO_ERROR,
    ERROR_OUT_CLOSE,
    SYNTAX_TOOMANYARGS
} Base64Error;


#define BASE64_MAX_MESSAGES 6
static const char* b64Messages[BASE64_MAX_MESSAGES] = {
            "b64:000:Invalid Message Code.",
            "b64:001:Syntax Error -- check help (-h) for usage.",
            "b64:002:File Error Opening/Creating Files.",
            "b64:003:File I/O Error -- Note: output file not removed.",
            "b64:004:Error on output file close.",
            "b64:005:Syntax: Too many arguments."
};


#define BASE64_MESSAGE(errorCode) ((errorCode > 0 && errorCode < BASE64_MAX_MESSAGES) ? b64Messages[errorCode] : b64Messages[0])


static void encodeBlock(uint8_t* in, uint8_t* out, int len) {
    out[0] = (uint8_t)cb64[(int)(in[0] >> 2)];
    out[1] = (uint8_t)cb64[(int)(((in[0] & 0x03) << 4) | ((in[1] & 0xf0) >> 4))];
    out[2] = (uint8_t)(len > 1 ? cb64[(int)(((in[1] & 0x0f) << 2) | ((in[2] & 0xc0) >> 6))] : '=');
    out[3] = (uint8_t)(len > 2 ? cb64[(int)(in[2] & 0x3f)] : '=');
}


static int encode(FILE* inFile, FILE* outFile) {
    uint8_t in[3];
    uint8_t out[4];
    int i, len = 0;
    int returnCode = 0;


    *in = (uint8_t)0;
    *out = (uint8_t)0;
    while (feof(inFile) == 0) {
        len = 0;
        for (i = 0; i < 3; i++) {
            in[i] = (uint8_t) getc(inFile);
            
            if (feof(inFile) == 0) {
                len++;
            } else {
                in[i] = (uint8_t)0;
            }
        }


        if (len > 0) {
            encodeBlock(in, out, len);
            for (i = 0; i < 4; i++) {
                if (putc((int)(out[i]), outFile) == 0) {
                    if (ferror(outFile) != 0) {
                        perror(BASE64_MESSAGE(FILE_IO_ERROR));
                        returnCode = FILE_IO_ERROR;
                    }
                    break;
                }
            }
        }
    }
    return (returnCode);
}


static void decodeBlock(uint8_t* in, uint8_t* out) {
    out[0] = (uint8_t)(in[0] << 2 | in[1] >> 4);
    out[1] = (uint8_t)(in[1] << 4 | in[2] >> 2);
    out[2] = (uint8_t)(((in[2] << 6) & 0xc0) | in[3]);
}


static int decode(FILE* inFile, FILE* outFile) {
    int returnCode = 0;
    uint8_t in[4];
    uint8_t out[3];
    int v;
    int i, len;


    *in = (uint8_t)0;
    *out = (uint8_t)0;
    while (feof(inFile) == 0) {
        for (len = 0, i = 0; i < 4 && feof(inFile) == 0; i++) {
            v = 0;


            // Read a valid base64 char.
            while (feof(inFile) == 0 && v == 0) {
                v = getc(inFile);
                if (v != EOF) {
                    v = ((v < 43 || v > 122) ? 0 : (int)cd64[v - 43]);
                    if (v != 0) {
                        v = ((v == (int)'$') ? 0 : v - 61);
                    }
                }
            }


            if (feof(inFile) == 0) {
                len++;
                if (v != 0) {
                    in[i] = (uint8_t)(v - 1);
                }
            } else {
                in[i] = (uint8_t)0;
            }
        }


        if (len > 0) {
            decodeBlock(in, out);
            for (i = 0; i < len - 1; i++) {
                if (putc((int)(out[i]), outFile) == EOF) {
                    if (ferror(outFile) != 0) {
                        perror(BASE64_MESSAGE(FILE_IO_ERROR));
                        returnCode = FILE_IO_ERROR;
                    }
                    break;
                }
            }
        }
    }
    return returnCode;
}


static int base64(char opt, char* inFileName, char* outFileName) {
    FILE* inFile;
    int returnCode = FILE_ERROR;


    if (!inFileName) {
        inFile = stdin;
    } else {
        inFile = fopen(inFileName, "rb");
    }


    if (!inFile) {
        perror(inFileName);
    } else {
        FILE* outFile;
        if (!outFileName) {
            outFile = stdout;
        } else {
            outFile = fopen(outFileName, "wb");
        }


        if (!outFile) {
            perror(outFileName);
        } else {
            if (opt == 'e') {
                returnCode = encode(inFile, outFile);
            } else {
                returnCode = decode(inFile, outFile);
            }


            if (returnCode == 0) {
                if (ferror(inFile) != 0 || ferror(outFile) != 0) {
                    perror(BASE64_MESSAGE(FILE_IO_ERROR));
                    returnCode = FILE_IO_ERROR;
                }
            }


            if (outFile != stdout) {
                if (fclose(outFile) != 0) {
                    perror(BASE64_MESSAGE(ERROR_OUT_CLOSE));
                    returnCode = FILE_IO_ERROR;
                }
            }
        }
        if (inFile != stdin) {
            if (fclose(inFile) != 0) {
                perror(BASE64_MESSAGE(ERROR_OUT_CLOSE));
                returnCode = FILE_IO_ERROR;
            }
        }
    }


    return (returnCode);
}


/*
** showuse
**
** display usage information, help, version info
*/
static void showuse(int moreHelp) {
    {
        printf( "\n" );
        printf( "  base64   (Base64 Encode/Decode)\n" );
        printf( "  Usage:   base64 -option [<FileIn> [<FileOut>]]\n" );
        printf( "  Purpose: This program is a simple utility that implements\n" );
        printf( "           Base64 Content-Transfer-Encoding (RFC1113).\n" );
    }
    
    if (moreHelp == 0) {
        printf( "           Use -h option for additional help.\n" );
    } else {
        printf( "  Options: -e  encode to Base64   -h  This help text.\n" );
        printf( "           -d  decode from Base64 -?  This help text.\n" );
        printf( "  Returns: 0 = success.  Non-zero is an error code.\n" );
        printf( "  ErrCode: 1 = Bad Syntax, 2 = File Open, 3 = File I/O\n" );
        printf( "  Example: b64 -e binfile b64file     <- Encode to b64\n" );
        printf( "           b64 -d b64file binfile     <- Decode from b64\n" );
    }
}


#define THIS_OPT(ac, av) ((char)(ac > 1 ? av[1][0] == '-' ? av[1][1] : 0 : 0))


/**
 * main
 * 
 * entry of program
 */
int main(int argc, char* argv[]) {
    char opt = (char)0;
    int returnCode = 0;
    char* inFileName = NULL;
    char* outFileName = NULL;


    while (THIS_OPT(argc, argv) != (char)0) {
        switch (THIS_OPT(argc, argv)) {
            case '?':
            case 'h':
                opt = 'h';
                break;
            case 'e':
            case 'd':
                opt = THIS_OPT(argc, argv);
                break;
            default:
                opt = (char)0;
                break;
        }
        argv++;
        argc--;
    }
    
    if (argc > 3) {
        printf("%s\n", BASE64_MESSAGE(SYNTAX_TOOMANYARGS));
    }


    switch (opt) {
        case 'e':
        case 'd':
            inFileName = argc > 1 ? argv[1] : NULL;
            outFileName = argc > 2 ? argv[2] : NULL;
            returnCode = base64(opt, inFileName, outFileName);
            break;
        case 0:
            if (argv[1] == NULL) {
                showuse(0);
            } else {
                returnCode = SYNTAX_ERROR;
            }
            break;
        case 'h':
            showuse((int)opt);
            break;
    }


    if (returnCode != 0) {
        printf("%s\n", BASE64_MESSAGE(returnCode));
    }


    return (returnCode);
}