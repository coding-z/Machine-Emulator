#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// MEMORY
#define FORMATTED_MEMORY "memory_f.dat"
#define MEMORY "memory.dat"
#define WORD_TYPE short
#define WORD_BASE 16
#define NUM_MEM_WORDS 100
#define BUFFER_SIZE 15

// ERRORS
#define MEM_ACC_ERR -1
#define MEMF_ACC_ERR -2
#define PROG_ACC_ERR -3
#define BAD_PROGRAM -4
#define INV_INSTR -5
#define INV_STR -6
#define NO_LABEL -7
#define NO_OPCODE 0x10000

// INSTRUCTION FORMATS
#define BLK 0
#define L__ 1
#define _IR_ 2
#define _IL_ 3
#define _I_ 4
#define _D_ 5
#define _S_ 6

// INSTRUCTIONS
#define NUM_INSTR 8

#define RDI 0x10
#define RDS 0x11
#define PRTI 0x12
#define PRTS 0x13
#define B 0x40
#define BN 0x41
#define BZ 0x42
#define END 0x43


// Node structure for linked list of program labels
typedef struct label {
    char name[BUFFER_SIZE];
    size_t address;
    struct label *next;
} Label;


// Executes a TIMS program
int execute( size_t *instrPtr, WORD_TYPE *instrReg, WORD_TYPE *accumulator );
// Dump the register contents
void dump( size_t *instrPtr, WORD_TYPE *instrReg, WORD_TYPE *accumulator );
// Load a TIMS program to the memory file
int load_program( char programName[], size_t address );
// Assemble a TIMS assembly program
int assemble_program( char fileName[], char *mnemonics[], WORD_TYPE opcodes[] );
// Assemble TIMS assembly instructions to instruction words
int assemble_instruction( WORD_TYPE *instrWord, char instr[], char *mnemonics[], WORD_TYPE opcodes[], Label *header );
// Find and stores the addresses of all program labels
Label *compile_labels( FILE *program );
// Resolves any known program label references
size_t resolve_label( char reference[], Label *header );
// Determine the opcode of the given mnemonic
int get_opcode( char mnemonic[], char *mnemonics[], WORD_TYPE opcodes[] );
// Cleans a TIMS assembly instruction string
int string_clean( char instr[], char destArray[][BUFFER_SIZE] );
// Clears TIMS memory
int clear_mem( void );
// Syncs formatted memory
int sync_memf( FILE *memory );



int main(int argc, char *argv[]) {
    // Initialize valid TIMS commands
    char *commands[NUM_INSTR] = {"RDI", "RDS", "PRTI", "PRTS", "B", "BN", "BZ", "END"};
    WORD_TYPE codes[NUM_INSTR] = {RDI, RDS, PRTI, PRTS, B, BN, BZ, END};

    // Initialize TIMS registers
    size_t instructionPointer = 0x0;
    WORD_TYPE instructionRegister = 0x0;
    WORD_TYPE accumulator = 0x0;


    clear_mem();

    char programName[3*BUFFER_SIZE];
    size_t loadAddr = 0x0;

    for (size_t i = 0; i < argc; i++) {
        if (!strncmp(argv[i], "la-", 3)) {
            loadAddr = strtol(&argv[i][3], NULL, 10);
        } else {
            strcpy(programName, argv[i]);
        }
    }

    instructionPointer = loadAddr;
    int assemblyStatus = assemble_program(programName, commands, codes);
    if (!assemblyStatus) {
        load_program(programName, loadAddr);
        execute(&instructionPointer, &instructionRegister, &accumulator);
    }

    return 0;
}



// ______________________________
//           EXECUTION
// ______________________________



// Executes TIMS program from the word address in the instruction pointer
int execute( size_t *instrPtr, WORD_TYPE *instrReg, WORD_TYPE *accumulator ) {
    FILE *memory = fopen(MEMORY, "r+");     // Open memory
    if (NULL == memory) { return MEM_ACC_ERR; }

    WORD_TYPE ioIntBuff = 0x0;      // I/O integer buffer
    char ioStrBuff[10*BUFFER_SIZE]; // I/O string buffer
    WORD_TYPE memTransferBuff = 0x0;    // Memory-register transfer buffer

    size_t instrAddr = *instrPtr * sizeof(WORD_TYPE);   // Instruction byte-address
    size_t operAddr = 0x0;  // Operand byte-address

    fseek(memory, instrAddr, SEEK_SET); // Seek to program start

    WORD_TYPE opcode = 0x0; // Instruction components
    WORD_TYPE operand = 0x0;

    puts("\n_____Executing TIMS Program_____\n");

    fread(instrReg, sizeof(WORD_TYPE), 1, memory);  // Fetch first instruction
    opcode = *instrReg / 0x100;     // Decode
    operand = *instrReg % 0x100;
    operAddr = operand * sizeof(WORD_TYPE);     // Operand byte-address

    // Execute entire program
    while (END != opcode) {
        // Execute instruction
        switch (opcode) {
            // I/O
            case RDI:   // Read integer from terminal to memory
                scanf("%d", &ioIntBuff);
                fseek(memory, operAddr, SEEK_SET);
                fwrite(&ioIntBuff, sizeof(ioIntBuff), 1, memory);
                sync_memf(memory);
                break;
            case RDS:   // Read string from terminal to memory
                gets(ioStrBuff);
                fseek(memory, operAddr, SEEK_SET);
                fwrite(ioStrBuff, sizeof(ioStrBuff[0]), strlen(ioStrBuff), memory);
                sync_memf(memory);
                break;
            case PRTI:  // Print integer from memory
                fseek(memory, operAddr, SEEK_SET);
                fread(&ioIntBuff, sizeof(ioIntBuff), 1, memory);
                printf("%d\n", ioIntBuff);
                break;
            case PRTS:  // Print string from memory
                fseek(memory, operAddr, SEEK_SET);
                fread(ioStrBuff, sizeof(ioStrBuff[0]), 10*BUFFER_SIZE, memory);
                printf("%s\n", ioStrBuff);
                break;
            case B:     // Unconditional branch
                *instrPtr = operand - 1;
                break;
            case BN:    // Branch if accumulator negative
                if (*accumulator < 0) {
                    *instrPtr = operand - 1;
                }
                break;
            case BZ:    // Branch if accumulator zero
                if (!(*accumulator)) {
                    *instrPtr = operand - 1;
                }
                break;
        }

        // Fetch next instruction
        (*instrPtr)++;
        instrAddr = *instrPtr * sizeof(WORD_TYPE);
        fseek(memory, instrAddr, SEEK_SET);
        fread(instrReg, sizeof(WORD_TYPE), 1, memory);

        // Decode instruction
        opcode = *instrReg / 0x100;
        operand = *instrReg % 0x100;
        operAddr = operand * sizeof(WORD_TYPE);
    }

    if (fclose(memory)) { return MEM_ACC_ERR; } // Close memory

    puts("\n_____TIMS Execution Complete_____\n");
    dump(instrPtr, instrReg, accumulator);

    return 0;
}



// Dump the register contents
void dump( size_t *instrPtr, WORD_TYPE *instrReg, WORD_TYPE *accumulator ) {
    puts("\nREGISTERS:");
    printf("%-24s0x%02x\n", "Instruction Pointer", *instrPtr);
    printf("%-22s0x%04x\n", "Instruction Register", *instrReg);
    printf("%-22s0x%04x\n\n", "Accumulator", *accumulator);
    exit(EXIT_SUCCESS);
}



// ______________________________
//            ASSEMBLY
// ______________________________



// Assembles a TIMS programs
int assemble_program( char fileName[], char *mnemonics[], WORD_TYPE opcodes[] ) {
    char programName[3*BUFFER_SIZE];
    strcpy(programName, fileName);  // Copy program name

    // Get output file name
    char assembledName[3*BUFFER_SIZE];
    strcpy(assembledName, strtok(fileName, "."));
    strcat(assembledName, "Asm.");
    strcat(assembledName, strtok(NULL, "."));

    strcpy(fileName, assembledName);    // Return output file name in fileName[]

    FILE *program = fopen(programName, "r");    // Open program
    if (NULL == program) { return PROG_ACC_ERR; }

    FILE *assembledFile = fopen(assembledName, "w");    // Create/open output file
    if (NULL == assembledFile) { return PROG_ACC_ERR; }

    Label *header = compile_labels(program);    // Compile all program labels

    char readBuff[11*BUFFER_SIZE];              // Program read buffer
    WORD_TYPE writeBuff[BUFFER_SIZE] = {0x0};   // Assembled word write buffer

    size_t lineNum = 1;
    unsigned int numErrors = 0;

    // Read program
    while (!feof(program)) {
        fgets(readBuff, sizeof(readBuff), program);
        
        int asmWords = assemble_instruction(writeBuff, readBuff, mnemonics, opcodes, header);   // Assemble instruction
        
        switch (asmWords) {
            case 0: // Skip blank lines
                lineNum++;
                continue;
            case INV_INSTR: // Invalid instruction
                printf("Line %u - invalid instruction\n", lineNum);
                numErrors++;
                lineNum++;
                continue;
            case INV_STR:   // Invalid string literal
                printf("Line %u - invalid string\n", lineNum);
                numErrors++;
                lineNum++;
                continue;
        }
        
        fwrite(writeBuff, sizeof(WORD_TYPE), asmWords, assembledFile);  // Write words
        lineNum++;
    }

    if (fclose(program)) { return PROG_ACC_ERR; }   // Close files
    if (fclose(assembledFile)) { return PROG_ACC_ERR; }

    if (numErrors) {
        printf("\nFailed to assemble \"%s\": %u errors contained\n\n", programName, numErrors);
        return BAD_PROGRAM;
    } else {
        printf("\nAssembled \"%s\" to \"%s\"\n\n", programName, assembledName);
        return 0;
    }
}



// Assemble TIMS assembly instruction to TIMS instruction word
// Returns the number of instruction words assembled (multiple if string)
int assemble_instruction( WORD_TYPE *instrWord, char instruction[], char *mnenonics[], WORD_TYPE opcodes[], Label *header ) {
    // Clean instruction string and determine format
    char components[3][BUFFER_SIZE];
    int format = string_clean(instruction, components);
    
    int opcode = 0x0;
    int operand = 0x0;
    size_t numStrWordsAsm = 0;
    // Assemble instruction word
    switch (format) {
        case _IR_:  // Instruction, reference operand
            operand = resolve_label(components[2], header);
            opcode = get_opcode(components[1], mnenonics, opcodes);
            if (NO_OPCODE == opcode) { return INV_INSTR; }  // Invalid command error
            instrWord[0] = (WORD_TYPE) (opcode * 0x100 + operand);
            return 1;
        case _IL_:  // Instruction, operand literal
            if (!strncmp(components[2], "0x", 2) || !strncmp(components[2], "0X", 2)) {
                operand = strtol(&components[2][2], NULL, 16);  // Hexadecimal literal
            } else if (!strcmp(components[2], "0")) {
                operand = strtol(components[2], NULL, 8);   // Octal literal
            } else {
                operand = strtol(components[2], NULL, 10);  // Decimal literal
            }
            opcode = get_opcode(components[1], mnenonics, opcodes);
            if (NO_OPCODE == opcode) { return INV_INSTR; }  // Invalid command error
            instrWord[0] = (WORD_TYPE) (opcode * 0x100 + operand);
            return 1;
        case _I_:   // No-operand instruction
            opcode = get_opcode(components[1], mnenonics, opcodes);
            if (NO_OPCODE == opcode) { return INV_INSTR; }  // Invalid command error
            instrWord[0] = (WORD_TYPE) opcode * 0x100;
            return 1;
        case _D_:   // Data word
            if (!strncmp(components[1], "0x", 2) || !strncmp(components[1], "0X", 2)) {
                operand = strtol(&components[1][2], NULL, 16);  // Hexadecimal literal
            } else if (!strcmp(components[1], "0")) {
                operand = strtol(components[1], NULL, 8);   // Octal literal
            } else {
                operand = strtol(components[1], NULL, 10);  // Decimal literal
            }
            instrWord[0] = (WORD_TYPE) operand;
            return 1;
        case _S_:   // String data
            for (numStrWordsAsm = 0; numStrWordsAsm < (strlen(instruction) + 1) / 2; numStrWordsAsm++) {
                instrWord[numStrWordsAsm] = (WORD_TYPE) (instruction[2*numStrWordsAsm + 1] * 0x100 + instruction[2*numStrWordsAsm]);
            }
            return numStrWordsAsm;  
        case INV_STR:   // Invalid string literal
            instrWord[0] = (WORD_TYPE) 0x0;
            return INV_STR;
        case BLK:   // Ignore blank lines
        case L__:
            return 0;
        default:    // Unrecognized format
            instrWord[0] = (WORD_TYPE) 0x0;
            return INV_INSTR;
    }
}



// Finds and stores the addresses of all program labels
Label *compile_labels( FILE *program ) {
    if (NULL == program) { return NULL; }   // Ensure program file open
    // Create label list
    Label *header = NULL;
    Label *prevNode = NULL;
    // Instruction read buffers
    char buffer[3*BUFFER_SIZE];
    char segments[3][BUFFER_SIZE];

    fseek(program, 0, SEEK_SET);    // Seek to program start
    size_t lineNumber = 1;
    size_t instructionAddress = 0;
    char savedLabel[BUFFER_SIZE] = "\0"; // Buffer for label-only lines
    size_t savedLabelLine;

    // Read every program line
    while (!feof(program)) {
        fgets(buffer, sizeof(buffer), program);

        int segNum = string_clean(buffer, segments);    // Clean instruction and determine format
        if (BLK == segNum) {    // Skip empty lines
            lineNumber++;
            continue;
        }
        if (L__ == segNum) {    // If lone label
            savedLabelLine = lineNumber;
            strcpy(savedLabel, segments[0]); // Save label
            lineNumber++;   // Continue until next instruction
            continue;
        }
        if ('\0' != savedLabel[0] && '\0' == segments[0][0]) {
            // If saved label exists and no current label, associate saved label with current instruction
            strcpy(segments[0], savedLabel);
            strcpy(savedLabel, "\0");
        }

        // If label found
        if ('\0' != segments[0][0]) {
            size_t reference = resolve_label(segments[0], header); // Attempt label resolve

            // If no reference
            if (NO_LABEL == reference) {
                // Create new node
                Label *newNode = malloc(sizeof(Label));
                if (NULL != newNode) {
                    newNode->next = NULL;
                    strcpy(newNode->name, segments[0]);
                    newNode->address = instructionAddress++;
                }
                // Append node to list
                if (NULL != prevNode) {
                    prevNode->next = newNode;
                    prevNode = newNode;
                }
                if (NULL == header) {   // Update header once
                    header = newNode;
                    prevNode = newNode;
                }
            }
        }

        lineNumber++;   // Next line
    }

    if (strcmp("\0", savedLabel)) { // Warning for dangling labels
        printf("Warning: Line %u - dangling label \"%s\" ignored\n", savedLabelLine, savedLabel);
    }

    fseek(program, 0, SEEK_SET);    // Return seek to file beginning     

    return header;  // Return linked list header
}



// Returns the program address of a known label reference
size_t resolve_label( char reference[], Label *header ) {
    Label *node = header;
    // Search every list node
    while (NULL != node) {
        if (!strcmp(node->name, reference)) {
            return node->address;   // Return label address
        }
        node = node->next;
    }
    return NO_LABEL;    // No matching label
}



// Returns the TIMS opcode of the given mnemonic string
int get_opcode( char mnemonic[], char *mnemonics[], WORD_TYPE opcodes[] ) {
    // Search opcode
    for (size_t i = 0; i < NUM_INSTR; i++) {
        // If match return opcode
        if (!strcmp(mnemonics[i], mnemonic)) {
            return opcodes[i];
        }
    }
    return NO_OPCODE ; // Else no match
}



// Cleans a TIMS instruction into its instruction components
// Returns instruction format of string
// Any substring will be returned in instr[]
int string_clean( char instr[], char destArray[][BUFFER_SIZE] ) {
    // If opening quotes
    if (strcspn(instr, "\"") < strlen(instr)) {
        char *string = strpbrk(instr, "\"");    // Save string pointer
        string++;
        // If label before quotes
        if (strcspn(instr, ":") < strcspn(instr, "\"")) {
            char *label = strtok(instr, ":");   // Separate label
            label = strtok(label, " "); // Clean label
            strcpy(destArray[0], label);    // Store label
        } else {    // Else set label element to null
            strcpy(destArray[0], "\0");
        }
        // If Closing quotes
        if (strcspn(string, "\"") < strlen(string)) {
            string = strtok(string, "\"");  // Separate string contents
            strcpy(instr, string);  // Return string contents in instr[]
            return _S_; // Return format
        } else {    // Else invalid string
            return INV_STR;
        }
    }

    // Determine if label exists
    unsigned int strLen = strlen(instr);
    char *token = strtok(instr, ":");

    // If label
    if (strlen(token) < strLen) {
        char *label = token;    // Separate label
        token = strtok(NULL, ":");  // Get remaining components
        label = strtok(label, " "); // Clean label
        strcpy(destArray[0], label); // Store label
    } else {    // Else set label element to null
        strcpy(destArray[0], "\0");
    }

    // Separate remaining components
    unsigned int componentNum = 0;
    if (NULL != token) {
        token = strtok(token, " ");
    }

    while (NULL != token) {
        if (!isgraph(token[0])) { break; }  // Ignore non-graphable components

        // Remove trailing non-graphical characters
        for (size_t i = strlen(token) - 1; i >= 0; i--) {
            if (!isgraph(token[i])) {
                token[i] = '\0';
            } else { break; }
        }

        for (size_t i = 0; i < strlen(token); i++) {    // Convert to uppercase
            token[i] = toupper(token[i]);
        }

        strcpy(destArray[++componentNum], token);   // Store component
        token = strtok(NULL, " ");  // Get next component
    }

    // Determine instruction format
    switch (componentNum) {
        case 0:
            if ('\0' != destArray[0][0]) {
                return L__; // Label only
            } else {
                return BLK; // Blank line
            }
        case 1:
            if (isdigit(destArray[1][0])) {
                return _D_; // Data word
            } else {
                return _I_; // No-operand instruction
            }
        case 2:
            if (isdigit(destArray[2][0])) {
                return _IL_;    // Instruction with operand literal
            } else {
                return _IR_;    // Instruction with reference operand
            }
        default:
            return INV_INSTR;   // Unrecognized instruction format
    }
}



// ______________________________
//             MEMORY
// ______________________________



// Loads a program to TIMS memory
int load_program( char programName[], size_t address ) {
    FILE *program = fopen(programName, "r");    // Open program
    if (NULL == program) { return PROG_ACC_ERR; }

    FILE *memory = fopen(MEMORY, "r+"); // Open memory
    if (NULL == memory) { return MEM_ACC_ERR; }

    WORD_TYPE buffer = 0x0; // Instruction transfer buffer

    fseek(memory, address * sizeof(WORD_TYPE), SEEK_SET);   // Seek to given memory word address 

    // Load program instructions to memory
    fread(&buffer, sizeof(WORD_TYPE), 1, program);
    while (!feof(program)) {
        switch (buffer / 0x100) {   // Correct any memory addressing with the program load address
            case RDI:
            case RDS:
            case PRTI:
            case PRTS:
            case B:
            case BN:
            case BZ:
                buffer += (WORD_TYPE) address;
        }
        
        fwrite(&buffer, sizeof(WORD_TYPE), 1, memory);
        fread(&buffer, sizeof(WORD_TYPE), 1, program);
    }

    if (fclose(program)) { return PROG_ACC_ERR; }
    int syncReturn = sync_memf(memory);  // Sync MEMF and close MEM

    printf("\nLoaded \"%s\" to TIMS memory word %u\n\n", programName, address);

    return syncReturn;
}



// Synchronizes the formatted memory file to the functional memory file
int sync_memf( FILE *memory ) {
    long int memOffset = 0x0;  // Memory offset if open file passed

    if (NULL == memory) {   // Ensure memory open
        memory = fopen(MEMORY, "r");
        if (NULL == memory) { return MEM_ACC_ERR; }
    } else {
        memOffset = ftell(memory);  // Save previous file offset
    }

    fseek(memory, 0 ,SEEK_SET);     // Seek to first memory word
    
    WORD_TYPE buffer[NUM_MEM_WORDS] = {0x0};    // Initialize memory transfer buffer

    // Read TIMS memory contents
    if (NUM_MEM_WORDS != fread(buffer, sizeof(WORD_TYPE), NUM_MEM_WORDS, memory)) {
        return MEM_ACC_ERR;
    }

    if (!memOffset) {   // Close memory if was previously closed
        if (fclose(memory)) { return MEM_ACC_ERR; }
    } else {
        fseek(memory, memOffset, SEEK_SET); // Else return to previous offset
    }

    FILE *memf = fopen(FORMATTED_MEMORY, "r+"); // Open formatted memory file
    if (NULL == memf) { return MEMF_ACC_ERR; }

    // Write formatted contents
    fprintf(memf, "           0         1         2         3         4         5         6         7         8         9\n");
    for (size_t word = 0; word < NUM_MEM_WORDS; word++) {
        if (!(word % 10)) {
            fprintf(memf, "%3u", word);
        }
        fprintf(memf, "   0x%04x%c", buffer[word], (word + 1) % 10 ? ' ' : '\n');
    }

    if (fclose(memf)) { return MEMF_ACC_ERR; }  // Close memf
    
    return 0;
}



// Clears TIMS memory
int clear_mem( void ) {
    FILE *memory = fopen(MEMORY, "r+"); // Open memory file
    if (NULL == memory) { return MEM_ACC_ERR; }

    WORD_TYPE buffer[NUM_MEM_WORDS] = {0x0};    // Initialize memory write buffer

    // Clear memory file
    if (NUM_MEM_WORDS != fwrite(buffer, sizeof(WORD_TYPE), NUM_MEM_WORDS, memory)) {
        return MEM_ACC_ERR;
    }

    rewind(memory); // Rewind offset so sync_memf() will close memory

    int syncReturn = sync_memf(memory);  // Sync memf

    return syncReturn;
}