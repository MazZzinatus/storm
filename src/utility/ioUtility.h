/*
 * ioUtility.h
 *
 *  Created on: 17.10.2012
 *      Author: Thomas Heinemann
 */

#ifndef UTILITY_H_
#define UTILITY_H_

#include "src/models/Dtmc.h"

namespace mrmc  {

namespace utility {

/*!
    Creates a DOT file which provides the graph of the DTMC.

    Currently, only a version for DTMCs using probabilities of type double is provided.
    Adaptions for other types may be included later.

    @param  dtmc     The DTMC to output
    @param  filename The Name of the file to write in. If the file already exists,
                     it will be overwritten.

 */
void dtmcToDot(mrmc::models::Dtmc<double>* dtmc, std::string filename);

/*!
    Parses a transition file and a labeling file and produces a DTMC out of them.
    Note that the labeling file may have at most as many nodes as the transition file!

    @param tra_file String containing the location of the transition file (....tra)
    @param lab_file String containing the location of the labeling file (....lab)
    @returns The DTMC described by the two files.

 */
mrmc::models::Dtmc<double>* parseDTMC(const char* tra_file, const char* lab_file);

} //namespace utility

} //namespace mrmc

#endif /* UTILITY_H_ */