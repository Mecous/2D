#include "sirm.h"

#include <vector>

class SIRMsModel {
private:
    int M_num_sirms;
    std::vector< SIRM > M_sirm;
public:

    explicit
    SIRMsModel( int num_sirms = 1 );

    int numSIRMs() const
      {
          return M_num_sirms;
      }

    void setModuleName( const size_t index,
                        const std::string & name );


    /*! calculate an output for an input vector */
    double calculateOutput( const std::vector< double > & input );

    /*! specify the number of fuzzy partitions of an SIRM */
    void specifyNumPartitions( const int index_module,
                               const int num_partitions );

    /*! specify the range of the target function for each attribute */
    void specifyDomain( const int index_attribute,
                        const double min_domain,
                        const double max_domain );

    /*! train the SIRMs model */
    void train( const double target,
                const double actual );

    /*! save parameters of SIRMs */
    bool saveParameters( const std::string & prefix );

    /*! load parameters of SIRMs */
    bool loadParameters( const std::string & prefix );
};
