/**
 * RArray.c
 * Realization of C dynamic memory buffer, in Ray additions.
 * May use like array of sized elements.
 * Author Kucheruavyu Ilya (kojiba@ro.ru)
 * 2014 Ukraine Kharkiv
 *  _         _ _ _
 * | |       (_|_) |
 * | | _____  _ _| |__   __ _
 * | |/ / _ \| | | '_ \ / _` |
 * |   < (_) | | | |_) | (_| |
 * |_|\_\___/| |_|_.__/ \__,_|
 *          _/ |
 *         |__/
 **/

#include <RBuffer.h>
#include <RClassTable.h>

constructor(RBuffer)) {
    object = allocator(RBuffer);
    if(object != nil) {
        // allocation of buffer
        master(object, RByteArray) = makeRByteArray(startSizeOfRBufferDefault);
        if(master(object, RByteArray) != nil) {

            // allocation of sizes array
            object->sizesArray = RAlloc(sizeof(RRange) * sizeOfObjectsOfRBufferDefault);

            if(object->sizesArray  != nil) {
                object->classId     = registerClassOnce(toString(RBuffer));
                object->freePlaces  = sizeOfObjectsOfRBufferDefault;
                object->count       = 0;
                object->totalPlaced = 0;
            } else {
                RError("RBuffer. Allocation of sizes array failed.", object);

                // cleanup
                deleter(master(object, RByteArray), RByteArray);
            }
        } else {
            RError("RBuffer. Allocation of master RByteArray failed.", object);
        }
    }
    return object;
}

destructor(RBuffer) {
    // kills buffer
    deleter(master(object, RByteArray), RByteArray);
    // kills sizes
    deallocator(object->sizesArray);
}

printer(RBuffer) {
    size_t iterator;
    RPrintf("%s object - %p {\n", toString(RBuffer), object);
    RPrintf("\t Total   size : %lu (bytes)\n", master(object, RByteArray)->size);
    RPrintf("\t Placed  size : %lu (bytes)\n", object->totalPlaced);
    RPrintf("\t Free  places : %lu\n", object->freePlaces);
    RPrintf("\t Count objcts : %lu\n\n", object->count);
    forAll(iterator, object->count) {
        RPrintf("\t\t %lu : size - %lu\n", iterator, object->sizesArray[iterator].size);
        printByteArrayInHex(master(object, RByteArray)->array + object->sizesArray[iterator].start, object->sizesArray[iterator].size);
        RPrintf("\n");
    }
    RPrintf("} %s object - %p\n", toString(RBuffer), object);
}

#pragma mark Reallocation

method(RRange*, addSizeToSizes, RBuffer), size_t newSize) {
    if(newSize > object->count) {
        object->sizesArray = RReAlloc(object->sizesArray, newSize * sizeof(RRange));
        if (object->sizesArray != nil) {
            // add free places
            object->freePlaces = newSize - object->count;
        }
    } else {
        RError("RBuffer. Bad new size for Sizes", object);
    }
    return object->sizesArray;
}

method(RByteArray*, addSizeToMem, RBuffer), size_t newSize) {
    if(newSize > (object->totalPlaced)) {
        master(object, RByteArray)->array = RReAlloc(master(object, RByteArray)->array, newSize);
        if (master(object, RByteArray)->array != nil) {
            // set newSize
            master(object, RByteArray)->size = newSize;
        }
    } else {
        RError("RBuffer. Bad new size for memory", object);
    }
    return master(object, RByteArray);
}

method(void, flush, RBuffer)) {
    object->freePlaces  += object->count;
    object->count        = 0;
    object->totalPlaced  = 0;
}

method(RBuffer*, sizeToFit, RBuffer)) {
    master(object, RByteArray)->array = RReAlloc(master(object, RByteArray)->array, object->totalPlaced);
    object->sizesArray = RReAlloc(object->sizesArray, object->count * sizeof(size_t));
    object->freePlaces = 0;
    return object;
}

#pragma mark Workers

method(rbool, checkIndexWithError, RBuffer), size_t index) {
    if(index < object->count) {
        return yes;
    } else {
        RError("RBuffer. Bad index.", object);
        return no;
    }
}

#pragma mark Data operations

method(void, addData, RBuffer), pointer data, size_t sizeInBytes) {

    while(object->freePlaces == 0) {
        // add free to sizes
        $(object, m(addSizeToSizes, RBuffer)), object->count * sizeMultiplierOfRBufferDefault);
    }
    size_t counter = 1;
    while(sizeInBytes > master(object, RByteArray)->size - object->totalPlaced) {
        // add free to buffer
        $(object, m(addSizeToMem, RBuffer)), object->totalPlaced * sizeMultiplierOfRBufferDefault * counter);
        ++counter;
    }

    if(master(object, RByteArray)->array != nil
            && object->sizesArray != nil) {

        // add object
        RMemCpy(master(object, RByteArray)->array + object->totalPlaced, data, sizeInBytes);
        object->sizesArray[object->count].start = object->totalPlaced;
        object->sizesArray[object->count].size = sizeInBytes;

        object->totalPlaced += sizeInBytes;
        ++object->count;
        --object->freePlaces;
    }
}

method(pointer, getDataReference, RBuffer), size_t index) {
    if($(object, m(checkIndexWithError, RBuffer)), index) == yes) {
        return (master(object, RByteArray)->array + object->sizesArray[index].start);
    } else {
        return nil;
    }
}

method(pointer, getDataCopy, RBuffer), size_t index) {
    byte *result = nil;
    pointer *ref = $(object, m(getDataReference, RBuffer)), index);
    if(ref != nil) {
        result = RAlloc(object->sizesArray[index].size);
        if (result != nil) {
            RMemCpy(result, ref, object->sizesArray[index].size);
        } else {
            RError("RBuffer. Bad allocation on getDataCopy.", object);
        }
    }
    return result;
}

method(void, deleteDataAt, RBuffer), size_t index) {
    if($(object, m(checkIndexWithError, RBuffer)), index) == yes) {

        RMemMove(master(object, RByteArray)->array + object->sizesArray[index].start,
                 master(object, RByteArray)->array + object->sizesArray[index].start + object->sizesArray[index].size,
                 object->totalPlaced - object->sizesArray[index].size);

        RMemMove(object->sizesArray + index,
                 object->sizesArray + index + 1,
                 object->count - index);

        --object->count;
        ++object->freePlaces;
        object->totalPlaced -= object->sizesArray[index].size;
    }
}

#pragma mark File I/O

RBuffer* RBufferFromFile(const char *filename) {
    ssize_t  fileSize;
    byte    *buffer;
    FILE    *file   = RFOpen(filename, "rb");
    RBuffer *result = nil;

    if(file != nil) {
        RFSeek(file, 0, SEEK_END);
        fileSize = RFTell(file);
        if(fileSize > 0) {
            RRewind(file);
            buffer = RAlloc(fileSize * (sizeof(byte)));
            if(buffer != nil) {
                // create variables
                uint64_t *sizesArray = nil;
                uint64_t  iterator   = 0;
                uint64_t  sumBytes  = 0;
                // read all
                RFRead(buffer, sizeof(byte), (size_t) fileSize, file);
                RFClose(file);

                // begin parse raw bytes
                if(buffer[0] == 4) {
                    //fixme custom 32-to-64
                    uint32_t *tempRef = (uint32_t *) (buffer + 1);
                    sizesArray = (uint64_t *) tempRef;

                } else if(buffer[0] == 8
                        && sizeof(size_t) == 8) {
                    sizesArray = (uint64_t *) (buffer + 1);
                } else {
                    RErrStr "ERROR. RBufferFromFile. Bad size_t size - %u for current size_t - %lu", buffer[0], sizeof(size_t));
                }

                if(sizesArray != nil) {
                    // find terminating '\0' size
                    sumBytes += sizesArray[0];
                    while (sizesArray[iterator] != 0) {
                        ++iterator;
                        sumBytes += sizesArray[iterator];
                    }
                    RByteArray *array = allocator(RByteArray);
                    if (array != nil) {
                        array->size  = sumBytes;
                        array->array = buffer + 1 + (buffer[0] * (iterator + 1));

                        // processing
                        result = $(array, m(serializeToBuffer, RByteArray)), sizesArray);

                        // cleanup
                        deallocator(array);
                        deallocator(buffer);

                    }
                }
            } else {
                RErrStr "ERROR. RBufferFromFile. Bad allocation buffer for file \"%s\" of size \"%lu\".\n", filename, fileSize);
            }
        } else {
            RErrStr "ERROR. RBufferFromFile. Error read file \"%s\".\n", filename);
        }
    } else {
        RErrStr "ERROR. RBufferFromFile. Cannot open file \"%s\".\n", filename);
    }
    return result;
}

method(void, saveToFile, RBuffer), const char* filename) {
    FILE *file = fopen(filename, "wb+");

    if (file != nil) {
        size_t iterator;

        // dump fist byte sizeof(size_t)
        size_t result = sizeof(size_t);
        result = fwrite(&result, 1, 1, file);
        if(result != 1) {
            RError("RBuffer. Failed save init data to file. Breaking process.", object);
        }

        // create sizes array
        size_t *tempSizes = RAlloc(object->count * sizeof(size_t));
        if(tempSizes == nil) {
            RError("RBuffer. Allocation of temp sizes array failed.", object);
        } else {
            // custom copy
            forAll(iterator, object->count) {
                tempSizes[iterator] = object->sizesArray[iterator].size;
            }

            // dump sizes array
            result = RFWrite(tempSizes, sizeof(size_t), object->count, file);
            if (result !=  object->count) {
                RError("RBuffer. Failed save size array to file.", object);
            }

            // dump last size like 0
            result = 0;
            result = RFWrite(&result, sizeof(size_t), 1, file);
            if (result != 1) {
                RError("RBuffer. Failed save last sizes '\\0' to file.", object);
            }

            // dump body
            result = RFWrite(master(object, RByteArray)->array, object->totalPlaced, 1, file);
            if (result != 1) {
                RError("RBuffer. Failed save RBuffer body to file.", object);
            }

            // cleanup
            deallocator(tempSizes);
            fclose(file);
        }
    } else {
        RError("RBuffer. Failed save string to file, cant open file.", object);
    }
}

#pragma mark Addition to RByteArray

method(RBuffer *, serializeToBuffer, RByteArray), size_t *sizesArray) {
    size_t iterator = 0;

    // search end 0, compute length
    while(sizesArray[iterator] != 0) {
        ++iterator;
    }

    if(iterator != 0) {
        RBuffer *result = allocator(RBuffer);
        if(result != nil) {
            master(result, RByteArray) = $(object, m(copy, RByteArray)));
            if(master(result, RByteArray) != nil) {
                result->count = iterator;

                RRange *newSizesArray = RAlloc(sizeof(RRange) * result->count);
                if(newSizesArray != nil) {
                    size_t sum = 0;

                    // process size array into RRange array
                    forAll(iterator, result->count) {
                        newSizesArray[iterator].start = sum;
                        newSizesArray[iterator].size = sizesArray[iterator];
                        sum += newSizesArray[iterator].size;
                    }

                    // final operations
                    result->sizesArray  = newSizesArray;
                    result->totalPlaced = object->size;
                    result->freePlaces  = 0;
                    result->classId = registerClassOnce(toString(RBuffer));
                }
            }
        }
        return result;
    }
    return nil;
}