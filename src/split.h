/*
============================================================================
DELLY: Structural variant discovery by integrated PE mapping and SR analysis
============================================================================
Copyright (C) 2012 Tobias Rausch

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
============================================================================
Contact: Tobias Rausch (rausch@embl.de)
============================================================================
*/

#ifndef SPLIT_H
#define SPLIT_H

#include <iostream>
#include "gotoh.h"
#include "needle.h"

namespace torali
{

  struct AlignDescriptor {
    int32_t cStart;
    int32_t cEnd;
    int32_t rStart;
    int32_t rEnd;
    int32_t homLeft;
    int32_t homRight;
    float percId;
    
    AlignDescriptor() : cStart(0), cEnd(0), rStart(0), rEnd(0), homLeft(0), homRight(0), percId(0) {}
  };

  template<typename TBPoint, typename TCT>
  inline void
  _adjustOrientation(std::string&, TBPoint, TCT, SVType<DeletionTag>) 
  {
    //Nop
  }

  template<typename TBPoint, typename TCT>
  inline void
  _adjustOrientation(std::string&, TBPoint, TCT, SVType<InsertionTag>) 
  {
    //Nop
  }

  template<typename TBPoint, typename TCT>
  inline void
  _adjustOrientation(std::string&, TBPoint, TCT, SVType<DuplicationTag>) 
  {
    //Nop
  }

  template<typename TBPoint, typename TCT>
  inline void
  _adjustOrientation(std::string& sequence, TBPoint bpPoint, TCT ct, SVType<InversionTag>) 
  {
    if (((!ct) && (bpPoint)) || ((ct) && (!bpPoint))) reverseComplement(sequence);
  }

  template<typename TBPoint, typename TCT>
  inline void
  _adjustOrientation(std::string& sequence, TBPoint bpPoint, TCT ct, SVType<TranslocationTag>) 
  {
    if (((ct==0) && (bpPoint)) || ((ct==1) && (!bpPoint))) reverseComplement(sequence);
  }


  inline bool 
  _validSoftClip(bam1_t* rec, int& clipSize, int& splitPoint, bool& leadingSC, unsigned short qualCut) {
    // Check read-length
    if (rec->core.l_qseq < 35) return false;

    // Check for single soft-clip
    unsigned int numSoftClip = 0;
    uint32_t* cigar = bam_get_cigar(rec);
    for (unsigned int i = 0; i < rec->core.n_cigar; ++i) {
      if (bam_cigar_op(cigar[i]) == BAM_CSOFT_CLIP) {
	++numSoftClip;
	clipSize = bam_cigar_oplen(cigar[i]);
      }
    }
    if (numSoftClip != 1) return false;

    // Check clip size
    if (clipSize <= (int32_t) (log10(rec->core.l_qseq) * 10)) return false;
    
    // Get quality vector
    typedef std::vector<uint8_t> TQuality;
    TQuality quality;
    quality.resize(rec->core.l_qseq);
    uint8_t* qualptr = bam_get_qual(rec);
    for (int i = 0; i < rec->core.l_qseq; ++i) quality[i] = qualptr[i];
    
    // Get soft-clips
    unsigned int alen = 0;
    unsigned int lastIns = 0;
    unsigned int meanQuality = 0;
    for (unsigned int i = 0; i < rec->core.n_cigar; ++i) {
      if (bam_cigar_op(cigar[i]) == BAM_CMATCH) {
	alen += bam_cigar_oplen(cigar[i]) + lastIns;
	lastIns = 0;
      } else if (bam_cigar_op(cigar[i]) == BAM_CINS) {
	lastIns = bam_cigar_oplen(cigar[i]);   // Only add if followed by 'M'
      } else if (bam_cigar_op(cigar[i]) == BAM_CSOFT_CLIP) {
	if (!alen) leadingSC = true;
	else leadingSC = false;
	splitPoint = rec->core.pos + alen;
	unsigned int qualSum = 0;
	for(unsigned int i = alen; i < (alen+clipSize); ++i) qualSum += quality[i];
	meanQuality = qualSum / clipSize;
      }
    }
    //std::cerr << clipSize << ',' << meanQuality << ',' << splitPoint << std::endl;
    return (meanQuality >= (unsigned int) qualCut);
  }

  template<typename TBPoint, typename TCT>
  inline bool
  _validSCOrientation(TBPoint bpPoint, bool leadingSC, TCT, SVType<DeletionTag>) 
  {
    if (((!bpPoint) && (!leadingSC)) || ((bpPoint) && (leadingSC))) return true;
    else return false;
  }

  template<typename TBPoint, typename TCT>
  inline bool
  _validSCOrientation(TBPoint bpPoint, bool leadingSC, TCT, SVType<InsertionTag>) 
  {
    if (((!bpPoint) && (!leadingSC)) || ((bpPoint) && (leadingSC))) return true;
    else return false;
  }

  template<typename TBPoint, typename TCT>
  inline bool
  _validSCOrientation(TBPoint bpPoint, bool leadingSC, TCT, SVType<DuplicationTag>) 
  {
    if (((!bpPoint) && (leadingSC)) || ((bpPoint) && (!leadingSC))) return true;
    else return false;
  }

  template<typename TBPoint, typename TCT>
  inline bool
  _validSCOrientation(TBPoint, bool leadingSC, TCT ct, SVType<InversionTag>) 
  {
    return (ct ? leadingSC : (!leadingSC));
  }

  template<typename TBPoint, typename TCT>
  inline bool
  _validSCOrientation(TBPoint bpPoint, bool leadingSC, TCT ct, SVType<TranslocationTag>) 
  {
    if (ct == 0) return (!leadingSC);
    else if (ct == 1) return leadingSC;
    else if (ct == 2) {
      if (((!bpPoint) && (!leadingSC)) || ((bpPoint) && (leadingSC))) return true;
      else return false;
    } 
    else if (ct == 3) {
      if (((!bpPoint) && (leadingSC)) || ((bpPoint) && (!leadingSC))) return true;
      else return false;
    } else return false;
  }


  // Deletions
  template<typename TSeq, typename TSVRecord, typename TRef>
  inline std::string
  _getSVRef(TSeq const* const ref, TSVRecord const& svRec, TRef const, SVType<DeletionTag>) {
    return boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd)) + boost::to_upper_copy(std::string(ref + svRec.svEndBeg, ref + svRec.svEndEnd));
  }

  // Insertions
  template<typename TSeq, typename TSVRecord, typename TRef>
  inline std::string
  _getSVRef(TSeq const* const ref, TSVRecord const& svRec, TRef const, SVType<InsertionTag>) {
    return boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svEndEnd));
  }

  // Duplications
  template<typename TSeq, typename TSVRecord, typename TRef>
  inline std::string
  _getSVRef(TSeq const* const ref, TSVRecord const& svRec, TRef const, SVType<DuplicationTag>) {
    return boost::to_upper_copy(std::string(ref + svRec.svEndBeg, ref + svRec.svEndEnd)) + boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd));
  }

  // Inversions
  template<typename TSeq, typename TSVRecord, typename TRef>
  inline std::string
  _getSVRef(TSeq const* const ref, TSVRecord const& svRec, TRef const, SVType<InversionTag>) {
    if (!svRec.ct) {
      std::string strEnd=boost::to_upper_copy(std::string(ref + svRec.svEndBeg, ref + svRec.svEndEnd));
      std::string strRevComp=strEnd;
      std::string::reverse_iterator itR = strEnd.rbegin();
      std::string::reverse_iterator itREnd = strEnd.rend();
      for(unsigned int i = 0; itR!=itREnd; ++itR, ++i) {
	switch (*itR) {
	case 'A': strRevComp[i]='T'; break;
	case 'C': strRevComp[i]='G'; break;
	case 'G': strRevComp[i]='C'; break;
	case 'T': strRevComp[i]='A'; break;
	case 'N': strRevComp[i]='N'; break;
	default: break;
	}
      }
      return boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd)) + strRevComp;
    } else {
      std::string strStart=boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd));
      std::string strRevComp=strStart;
      std::string::reverse_iterator itR = strStart.rbegin();
      std::string::reverse_iterator itREnd = strStart.rend();
      for(unsigned int i = 0; itR!=itREnd; ++itR, ++i) {
	switch (*itR) {
	case 'A': strRevComp[i]='T'; break;
	case 'C': strRevComp[i]='G'; break;
	case 'G': strRevComp[i]='C'; break;
	case 'T': strRevComp[i]='A'; break;
	case 'N': strRevComp[i]='N'; break;
	default: break;
	}
      }
      return strRevComp + boost::to_upper_copy(std::string(ref + svRec.svEndBeg, ref + svRec.svEndEnd));
    }
  }

  // Translocations
  template<typename TSeq, typename TSVRecord, typename TRef>
  inline std::string
  _getSVRef(TSeq const* const ref, TSVRecord const& svRec, TRef const refIndex, SVType<TranslocationTag>) {
    if (svRec.chr==refIndex) {
      if ((svRec.ct==0) || (svRec.ct == 2)) return boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd)) + svRec.consensus;
      else if (svRec.ct == 1) {
	std::string strEnd=boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd));
	std::string refPart=strEnd;
	std::string::reverse_iterator itR = strEnd.rbegin();
	std::string::reverse_iterator itREnd = strEnd.rend();
	for(unsigned int i = 0; itR!=itREnd; ++itR, ++i) {
	  switch (*itR) {
	  case 'A': refPart[i]='T'; break;
	  case 'C': refPart[i]='G'; break;
	  case 'G': refPart[i]='C'; break;
	  case 'T': refPart[i]='A'; break;
	  case 'N': refPart[i]='N'; break;
	  default: break;
	  }
	}
	return refPart + svRec.consensus;
      } else return svRec.consensus + boost::to_upper_copy(std::string(ref + svRec.svStartBeg, ref + svRec.svStartEnd));
    } else {
      // chr2
      if (svRec.ct==0) {
	std::string strEnd=boost::to_upper_copy(std::string(ref + svRec.svEndBeg, ref + svRec.svEndEnd));
	std::string refPart=strEnd;
	std::string::reverse_iterator itR = strEnd.rbegin();
	std::string::reverse_iterator itREnd = strEnd.rend();
	for(unsigned int i = 0; itR!=itREnd; ++itR, ++i) {
	  switch (*itR) {
	  case 'A': refPart[i]='T'; break;
	  case 'C': refPart[i]='G'; break;
	  case 'G': refPart[i]='C'; break;
	  case 'T': refPart[i]='A'; break;
	  case 'N': refPart[i]='N'; break;
	  default: break;
	  }
	}
	return refPart;
      } else return boost::to_upper_copy(std::string(ref + svRec.svEndBeg, ref + svRec.svEndEnd));
    }
  }


  // Deletions
  template<typename TString, typename TSvRecord, typename TAlignDescriptor, typename TPosition>
  inline bool
  _coordTransform(TString const&, TSvRecord const& sv, TAlignDescriptor const& ad, TPosition& finalGapStart, TPosition& finalGapEnd, SVType<DeletionTag>) {
    int32_t annealed = sv.svStartEnd - sv.svStartBeg;
    if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
    finalGapStart = sv.svStartBeg + ad.rStart;
    finalGapEnd = sv.svEndBeg + (ad.rEnd - annealed);
    return true;
  }

  // Duplications
  template<typename TString, typename TSvRecord, typename TAlignDescriptor, typename TPosition>
  inline bool
  _coordTransform(TString const&, TSvRecord const& sv, TAlignDescriptor const& ad, TPosition& finalGapStart, TPosition& finalGapEnd, SVType<DuplicationTag>) {
    int32_t annealed = sv.svEndEnd - sv.svEndBeg;
    if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
    finalGapStart = sv.svStartBeg + (ad.rEnd - annealed);
    finalGapEnd = sv.svEndBeg + ad.rStart;
    return true;
  }

  // Inversion
  template<typename TString, typename TSvRecord, typename TAlignDescriptor, typename TPosition>
  inline bool
  _coordTransform(TString const& ref, TSvRecord const& sv, TAlignDescriptor const& ad, TPosition& finalGapStart, TPosition& finalGapEnd, SVType<InversionTag>) {
    int32_t annealed = sv.svStartEnd - sv.svStartBeg;
    if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
    if (!sv.ct) {
      finalGapStart = sv.svStartBeg + ad.rStart;
      finalGapEnd = sv.svEndBeg + (ref.size() - ad.rEnd) + 1;
    } else {
      finalGapStart = sv.svStartBeg + (annealed - ad.rStart) + 1;
      finalGapEnd = sv.svEndBeg + (ad.rEnd - annealed);
    } 
    return true;
  }

  // Translocation
  template<typename TString, typename TSvRecord, typename TAlignDescriptor, typename TPosition>
  inline bool
  _coordTransform(TString const& ref, TSvRecord const& sv, TAlignDescriptor const& ad, TPosition& finalGapStart, TPosition& finalGapEnd, SVType<TranslocationTag>) {
    if (sv.ct == 0) {
      int32_t annealed = sv.svStartEnd - sv.svStartBeg;
      if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
      finalGapStart = sv.svStartBeg + ad.rStart;
      finalGapEnd = sv.svEndBeg + (ref.size() - ad.rEnd) + 1;
    }
    else if (sv.ct == 1) {
      int32_t annealed = sv.svStartEnd - sv.svStartBeg;
      if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
      finalGapStart = sv.svStartBeg + (annealed - ad.rStart) + 1;
      finalGapEnd = sv.svEndBeg + (ad.rEnd - annealed);
    }
    else if (sv.ct == 2) {
      int32_t annealed = sv.svStartEnd - sv.svStartBeg;
      if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
      finalGapStart = sv.svStartBeg + ad.rStart;
      finalGapEnd = sv.svEndBeg + (ad.rEnd - annealed);
    } 
    else if (sv.ct == 3) {
      int32_t annealed = sv.svEndEnd - sv.svEndBeg;
      if ((ad.rStart >= annealed) || (ad.rEnd < annealed)) return false;
      finalGapStart = sv.svStartBeg + (ad.rEnd - annealed);
      finalGapEnd = sv.svEndBeg + ad.rStart;
    }
    else return false;
    return true;
  }

  template<typename TString, typename TSvRecord, typename TAlignDescriptor, typename TPosition>
  inline bool
  _coordTransform(TString const&, TSvRecord const& sv, TAlignDescriptor const& ad, TPosition& finalGapStart, TPosition& finalGapEnd, SVType<InsertionTag>) {
    finalGapStart = sv.svStartBeg + ad.rStart;
    finalGapEnd = sv.svStartBeg + ad.rEnd;
    return true;
  }


  template<typename TPos, typename TTag>
  inline bool
  _validSRAlignment(TPos const cStart, TPos const cEnd, TPos const rStart, TPos const rEnd, SVType<TTag>)
  {
    return (((cEnd - cStart) < 5) && ((rEnd - rStart) > 15));
  }

  template<typename TPos>
  inline bool
  _validSRAlignment(TPos const cStart, TPos const cEnd, TPos const rStart, TPos const rEnd, SVType<InsertionTag>)
  {
    return (((rEnd - rStart) < 5) && ((cEnd - cStart) > 15));
  }

  template<typename TGap, typename TTag>
  inline bool
  _checkSVGap(TGap const refGap, TGap const oldRefGap, TGap const, TGap const, SVType<TTag>) {
    return (refGap > oldRefGap);
  }

  template<typename TGap>
  inline bool
  _checkSVGap(TGap const, TGap const, TGap const varGap, TGap const oldVarGap, SVType<InsertionTag>) {
    return (varGap > oldVarGap);
  }


  template<typename TAlign, typename TAIndex, typename TFloat>
  inline void
  _percentIdentity(TAlign const& align, TAIndex const gS, TAIndex const gE, TFloat& percId) {
    // Find percent identity
    bool varSeen = false;
    bool refSeen = false;
    uint32_t gapMM = 0;
    uint32_t mm = 0;
    uint32_t ma = 0;
    bool inGap=false;
    for(TAIndex j = 0; j < (TAIndex) align.shape()[1]; ++j) {
      if ((j < gS) || (j > gE)) {
	if (align[0][j] != '-') varSeen = true;
	if (align[1][j] != '-') refSeen = true;
	// Internal gap?
	if ((align[0][j] == '-') || (align[1][j] == '-')) {
	  if ((refSeen) && (varSeen)) {
	    if (!inGap) {
	      inGap = true;
	      gapMM = 0;
	    }
	    gapMM += 1;
	  }
	} else {
	  if (inGap) {
	    mm += gapMM;
	    inGap=false;
	  }
	  if (align[0][j] == align[1][j]) ma += 1;
	  else mm += 1;
	}
      }
    }
    percId = (TFloat) ma / (TFloat) (ma + mm);
  }


  template<typename TAlign, typename TAIndex, typename TLength>
  inline void
  _findHomology(TAlign const& align, TAIndex const gS, TAIndex const gE, TLength& homLeft, TLength& homRight) {
    int32_t mmThres = 1;
    if (align[1][gS] == '-') {
      // Insertion
      int32_t mismatch = 0;
      int32_t offset = 0;
      for(TAIndex i = 0; i < gS; ++i, ++homLeft) {
	if (align[1][gS-i-1] != align[0][gE-i-offset]) ++mismatch;
	if (mismatch > mmThres) {
	  // Try 1bp insertion
	  if (!offset) {
	    if (align[1][gS-i-1] == align[0][gE-i-(++offset)]) {
	      --mismatch;
	      continue;
	    }
	  }
	  break;
	}
      }
      mismatch = 0;
      offset = 0;
      for(TAIndex i = 0; i < (TAIndex) (align.shape()[1] - gE - 1); ++i, ++homRight) {
	if (align[0][gS+i] != align[1][gE+i+1]) ++mismatch;
	if (mismatch > mmThres) {
	  // Try 1bp insertion
	  if (!offset) {
	    if (align[0][gS+i+(++offset)] == align[1][gE+i+1]) {
	      --mismatch;
	      continue;
	    }
	  }
	  break;
	}
      }
    } else if (align[0][gS] == '-') {
      // Deletion
      int32_t mismatch = 0;
      int32_t offset = 0;
      for(TAIndex i = 0; i < gS; ++i, ++homLeft) {
	if (align[0][gS-i-1] != align[1][gE-i-offset]) ++mismatch;
	if (mismatch > mmThres) {
	  // Try 1bp deletion
	  if (!offset) {
	    if (align[0][gS-i-1] == align[1][gE-i-(++offset)]) {
	      --mismatch;
	      continue;
	    }
	  }
	  break;
	}
      }
      mismatch = 0;
      offset = 0;
      for(TAIndex i = 0; i < (TAIndex) (align.shape()[1] - gE - 1); ++i, ++homRight) {
	if (align[1][gS+i] != align[0][gE+i+1]) ++mismatch;
	if (mismatch > mmThres) {
	  // Try 1bp deletion
	  if (!offset) {
	    if (align[1][gS+i+(++offset)] == align[0][gE+i+1]) {
	      --mismatch;
	      continue;
	    }
	  }
	  break;
	}
      }
    }
  }

  
  template<typename TConfig, typename TAlign, typename TAlignDescriptor, typename TSVType>
  inline bool
  _findSplit(TConfig const& c, TAlign const& align, TAlignDescriptor& ad, TSVType svt) {
    // Initializiation
    int32_t gS=0;
    int32_t gE=0;

    // Find longest internal gap
    int32_t refIndex=0;
    int32_t varIndex=0;
    int32_t gapStartRefIndex=0;
    int32_t gapStartVarIndex=0;
    int32_t a1 = 0;
    bool inGap=false;
    for(int32_t j = 0; j < (int32_t) align.shape()[1]; ++j) {
      if (align[0][j] != '-') ++varIndex;
      if (align[1][j] != '-') ++refIndex;
      // Internal gap?
      if (((align[0][j] == '-') || (align[1][j] == '-')) && (refIndex>0) && (varIndex>0)) {
	if (!inGap) {
	  gapStartVarIndex = (align[0][j] != '-') ? (varIndex - 1) : varIndex;
	  gapStartRefIndex = (align[1][j] != '-') ? (refIndex - 1) : refIndex;
	  a1 = j;
	  inGap = true;
	}
      } else {
	if ((inGap) && (_checkSVGap((refIndex - gapStartRefIndex), (ad.rEnd - ad.rStart), (varIndex - gapStartVarIndex), (ad.cEnd - ad.cStart), svt))) {
	  ad.rStart=gapStartRefIndex;
	  ad.rEnd=refIndex;
	  ad.cStart=gapStartVarIndex;
	  ad.cEnd=varIndex;
	  gS = a1;
	  gE = j - 1;
	}
	inGap=false;
      }
    }
    if (ad.rEnd <= ad.rStart) return false;
    
    // Is this a valid split-read alignment?
    if (!_validSRAlignment(ad.cStart, ad.cEnd, ad.rStart, ad.rEnd, svt)) return false;

    // Check percent identity
    _percentIdentity(align, gS, gE, ad.percId);
    if (ad.percId < c.flankQuality) return false;

    // Find homology
    _findHomology(align, gS, gE, ad.homLeft, ad.homRight);

    // Check flanking alignment length
    if ((ad.homLeft + c.minimumFlankSize > ad.cStart) || ( varIndex < ad.cEnd + ad.homRight + c.minimumFlankSize)) return false;
    if ((ad.homLeft + c.minimumFlankSize > ad.rStart) || ( refIndex < ad.rEnd + ad.homRight + c.minimumFlankSize)) return false;

    // Valid split-read alignment
    return true;
  }

  template<typename TAlign, typename TTag>
  inline bool
  _consRefAlignment(std::string const& cons, std::string const& svRefStr, TAlign& aln, SVType<TTag>)
  {
    AlignConfig<true, false> semiglobal;
    DnaScore<int> lnsc(5, -4, -4, -4);
    bool reNeedle = longNeedle(cons, svRefStr, aln, semiglobal, lnsc);
    return reNeedle;
  }

  template<typename TAlign>
  inline bool
  _consRefAlignment(std::string const& cons, std::string const& svRefStr, TAlign& aln, SVType<InsertionTag>)
  {
    typedef typename TAlign::index TAIndex;
    AlignConfig<false, true> semiglobal;
    DnaScore<int> lnsc(5, -4, -4, -4);
    bool reNeedle = longNeedle(svRefStr, cons, aln, semiglobal, lnsc);
    for(TAIndex j = 0; j < (TAIndex) aln.shape()[1]; ++j) {
      char tmp = aln[0][j];
      aln[0][j] = aln[1][j];
      aln[1][j] = tmp;
    }	
    return reNeedle;
  }

  template<typename TConfig, typename TStructuralVariantRecord, typename TTag>
  inline bool
  alignConsensus(TConfig const& c, TStructuralVariantRecord& sv, std::string const& svRefStr, SVType<TTag> svType) {
    if ( (int32_t) sv.consensus.size() < (2 * c.minimumFlankSize)) return false;

    // Consensus to reference alignment
    typedef boost::multi_array<char, 2> TAlign;
    TAlign align;
    if (!_consRefAlignment(sv.consensus, svRefStr, align, svType)) return false;

    // Check breakpoint
    AlignDescriptor ad;
    if (!_findSplit(c, align, ad, svType)) return false;

    // Debug consensus to reference alignment
    //for(TAIndex i = 0; i<align.shape()[0]; ++i) {
    //for(TAIndex j = 0; j<align.shape()[1]; ++j) {
    //std::cerr << align[i][j];
    //}
    //std::cerr << std::endl;
    //}
    //std::cerr << std::endl;

    // Get the start and end of the structural variant
    unsigned int finalGapStart = 0;
    unsigned int finalGapEnd = 0;
    if (c.technology == "illumina") {
      if (!_coordTransform(svRefStr, sv, ad, finalGapStart, finalGapEnd, svType)) return false;
    } else if (c.technology == "pacbio") {
      int32_t rs = std::max(0, sv.svStart - (int32_t) (sv.consensus.size()));
      finalGapStart = rs + ad.rStart - 1;
      finalGapEnd = rs + ad.rEnd - 1;
    }

    // Set breakpoint & quality
    sv.precise=true;
    sv.svStart=finalGapStart;
    sv.svEnd=finalGapEnd;
    sv.srAlignQuality = ad.percId;
    sv.insLen=ad.cEnd - ad.cStart - 1;
    return true;
  }



}

#endif
