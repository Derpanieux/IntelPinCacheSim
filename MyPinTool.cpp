/*
 * Copyright lol no copyright i aint got a lawyer
 * Made by Kasey Tian
 */

/*! @file
 *  This is a Pin tool to simulate a single level simple cache for
 *  the purpose of learning how to create a Pin tool.
 */

#include "pin.H"
#include <iostream>
#include <fstream>
using std::cerr;
using std::endl;
using std::string;
using std::__1::to_string;

/* ================================================================== */
// Constants
/* ================================================================== */


/* ================================================================== */
// Global variables
/* ================================================================== */
//cache parameters
static uintptr_t sets; //number of sets in the cache
static uintptr_t associativity; //number of elements per set
static uintptr_t blockSize; //number of bytes per block
static uintptr_t** cache; //array to keep track of what is in cache
static UINT64** lastAccess; //array to keep track of when each entry was last accessed
static bool** validBits; //array to keep track of valid bits

//logging total mem ops
static UINT64 hitCount = 0; //number of cache hits
static UINT64 missCount = 0; //number of cache misses
static UINT64 accessCount = 0; //total number of mem ops

//logging reads
static UINT64 readHitCount = 0; //number of cache hits on read
static UINT64 readMissCount = 0; //number of cache misses on read
static UINT64 readCount = 0; //total number of reads

//logging writes
static UINT64 writeHitCount = 0; //number of cache hits on write
static UINT64 writeMissCount = 0; //number of cache misses on write
static UINT64 writeCount = 0; //total number of writes



std::ostream* out = &cerr;

/* ===================================================================== */
// Command line switches
/* ===================================================================== */
KNOB< string > KnobOutputFile(KNOB_MODE_WRITEONCE, "pintool", "o", "MyPinTool.out", "specify file name for output");
KNOB< uintptr_t > KnobSetCount(KNOB_MODE_WRITEONCE, "pintool", "s", "8192", "specify the number of sets in the cache");
KNOB< uintptr_t > KnobAssociativity(KNOB_MODE_WRITEONCE, "pintool", "a", "4", "specify the number of elements in each set");
KNOB< uintptr_t > KnobBlockSize(KNOB_MODE_WRITEONCE, "pintool", "b", "64", "specify the number of bytes in a block");

/* ===================================================================== */
// Utilities
/* ===================================================================== */

/*!
 *  Print out help message.
 */
INT32 Usage()
{
    cerr << "This tool simulates a basic, single level cache with an LRU" << endl
         << "replacement strategy. It reports the performance of the cache." << endl
         << endl;

    cerr << KNOB_BASE::StringKnobSummary() << endl;

    return -1;
}
//return a stringified percentage of a numerator and denominator, truncated to 2 decimal places
string percent(UINT64 num, UINT64 denom) {
    UINT64 perTenThousand = num*10000 / denom;
    UINT64 percent = perTenThousand / 100;
    UINT64 rem = perTenThousand % 100;
    string str = to_string(percent);
    str.append(".");
    str.append(to_string(rem));
    str.append("%");
    return str;
}

/* ===================================================================== */
// Analysis routines
/* ===================================================================== */

/*!
 * Determine if an access is a hit or not
 */
bool checkHit(void* ip, void* addr) {
    uintptr_t pointer = (uintptr_t)(addr);
    uintptr_t cacheIndex = pointer / blockSize;
    uintptr_t tag = cacheIndex / sets;
    cacheIndex %= sets;
    //check for cache hit in every item in set
    for(uintptr_t i = 0; i < sets; i++) {
        if(validBits[cacheIndex][i] && (cache[cacheIndex][i] == tag)) {
            //cache hit
            lastAccess[cacheIndex][i] = accessCount; //update most recent access
            return true;
        }
    }
    //cache miss
    uintptr_t mini = 0;
    for(uintptr_t i = 1; i < sets; i++) {
        if(lastAccess[cacheIndex][i] < lastAccess[cacheIndex][mini]) {
            mini = i;
        }
    }
    cache[cacheIndex][mini] = tag;
    lastAccess[cacheIndex][mini] = accessCount;
    return false;
}

//Run on memory read
VOID onMemRead(VOID* ip, VOID* addr) {
    if(checkHit(ip, addr)) {
        readHitCount++;
        hitCount++;
    }else {
        readMissCount++;
        missCount++;
    }
    readCount++;
    accessCount++;
}

//Run on memory write
VOID onMemWrite(VOID* ip, VOID* addr) {
    if(checkHit(ip, addr)) {
        writeHitCount++;
        hitCount++;
    }else {
        writeMissCount++;
        missCount++;
    }
    writeCount++;
    accessCount++;
}

/* ===================================================================== */
// Instrumentation callbacks
/* ===================================================================== */

/*!
 * Log cache hits and misses for memory instructions
 * This function is called every time a new trace is encountered.
 * @param[in]   ins      intruction to be instrumented
 * @param[in]   v        value specified by the tool in the Ins_AddInstrumentFunction
 *                       function call
 */
VOID Instruction(INS ins, VOID* v) {
    //get memory operands
    UINT32 memOperands = INS_MemoryOperandCount(ins);

    //iterate over memory operands
    for (UINT32 memOp = 0; memOp < memOperands; memOp++) {
        if (INS_MemoryOperandIsRead(ins, memOp)) {
            //if read, call read function
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)onMemRead, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_END);
        }
        if (INS_MemoryOperandIsWritten(ins, memOp)) {
            //if write, call write function
            INS_InsertPredicatedCall(ins, IPOINT_BEFORE, (AFUNPTR)onMemWrite, IARG_INST_PTR, IARG_MEMORYOP_EA, memOp, IARG_END);
        }
        //not we cannot do if/else here because some instructions in x86 and x64 perform read and write in a signle instruction
    }
}
/*!
 * Print out analysis results.
 * This function is called when the application exits.
 * @param[in]   code            exit code of the application
 * @param[in]   v               value specified by the tool in the 
 *                              PIN_AddFiniFunction function call
 */
VOID Fini(INT32 code, VOID* v)
{
    *out << "===============================================" << endl;
    *out << "Cache parameters" << endl;
    *out << "# of sets      : " << sets << endl;
    *out << "#-way sets     : " << associativity << endl;
    *out << "block size     : " << blockSize << endl;
    *out << "Total hits     : " << hitCount << endl;
    *out << "===============================================" << endl;
    *out << "Cache simulation results" << endl;
    *out << "Total misses   : " << missCount << endl;
    *out << "Total accesses : " << accessCount << endl;
    *out << "Total miss rate: " << percent(missCount, accessCount) << endl;
    *out << "Read hits      : " << readHitCount << endl;
    *out << "Read misses    : " << readMissCount << endl;
    *out << "Read accesses  : " << readCount << endl;
    *out << "Read miss rate : " << percent(readMissCount, readCount)  << endl;
    *out << "Write hits     : " << writeHitCount << endl;
    *out << "Write misses   : " << writeMissCount << endl;
    *out << "Write accesses : " << writeCount << endl;
    *out << "Write miss rate: " << percent(writeMissCount, writeCount)  << endl;
    *out << "===============================================" << endl;
}

/*!
 * The main procedure of the tool.
 * This function is called when the application image is loaded but not yet started.
 * @param[in]   argc            total number of elements in the argv array
 * @param[in]   argv            array of command line arguments, 
 *                              including pin -t <toolname> -- ...
 */
int main(int argc, char* argv[])
{
    // Initialize PIN library. Print help message if -h(elp) is specified
    // in the command line or the command line is invalid
    if (PIN_Init(argc, argv))
    {
        return Usage();
    }

    //init file output
    string fileName = KnobOutputFile.Value();
    if (!fileName.empty())
    {
        out = new std::ofstream(fileName.c_str());
    }

    //init cache parameters
    sets = KnobSetCount.Value();
    associativity = KnobAssociativity.Value();
    blockSize = KnobBlockSize.Value();
    cache = new uintptr_t*[sets];
    lastAccess = new UINT64*[sets];
    validBits = new bool*[sets];
    for(uintptr_t i = 0; i < sets; i++) {
        cache[i] = new uintptr_t[associativity];
        lastAccess[i] = new UINT64[associativity];
        validBits[i] = new bool[associativity];
        for(uintptr_t j = 0; j < associativity; j++) {
            validBits[i][j] = false;
        }
    }

    // Register function to be called to instrument traces
    INS_AddInstrumentFunction(Instruction, 0);
    PIN_AddFiniFunction(Fini, 0);

    cerr << "===============================================" << endl;
    cerr << "This application is instrumented by MyPinTool" << endl;
    if (!KnobOutputFile.Value().empty())
    {
        cerr << "See file " << KnobOutputFile.Value() << " for analysis results" << endl;
    }
    cerr << "===============================================" << endl;

    // Start the program, never returns
    PIN_StartProgram();

    return 0;
}

/* ===================================================================== */
/* eof */
/* ===================================================================== */
