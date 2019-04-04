//
//  stat.hpp
//  ndnrtc
//
//  Created by Peter Gusev on 4/4/19.
//  Copyright © 2019 Regents of the University of California
//

#ifndef stat_hpp
#define stat_hpp

#include <stdio.h>
#include "../../include/statistics.hpp"

void
dumpStats(std::string csvFile, std::string stats,
          const ndnrtc::statistics::StatisticsStorage&);

#endif /* stat_hpp */
