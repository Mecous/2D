#include "defensive_sirms_model.h"

#include <iostream>

/*-------------------------------------------------------------------*/
/*!

 */
DefensiveSIRMsModel::DefensiveSIRMsModel( int num_sirms ):
    M_num_sirms( num_sirms )
{
    M_sirm.clear();
    M_sirm.resize( M_num_sirms );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefensiveSIRMsModel::setModuleName( const size_t index,
                           const std::string & name )
{
    if ( index >= M_sirm.size() )
    {
        std::cerr << __FILE__ << ' ' << __LINE__
                  << ": illegal module index " << index << std::endl;
        return;
    }

    M_sirm[index].setModuleName( name );
}

/*-------------------------------------------------------------------*/
/*!

 */
double
DefensiveSIRMsModel::calculateOutput( const std::vector< double > & input )
{
    double result = 0.0;

    int i = 0;
    for ( std::vector< DefensiveSIRM >::iterator it = M_sirm.begin(), end = M_sirm.end();
          it != end;
          ++it, ++i )
    {
        double y = it->calculateOutput( input[i] );

        /*
        std::cout << "Output from " << i << "-th module: " << y[i];
        std::cout << " weight: " << (*it).weight() << std::endl;
        */

        result += it->weight() * y;
    }

    return result;
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefensiveSIRMsModel::specifyNumPartitions( const int index_module,
                                  const int num_partitions )
{
    M_sirm[index_module].setNumPartitions( num_partitions );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefensiveSIRMsModel::train( const double target,
                   const double actual )
{
    for ( std::vector< DefensiveSIRM >::iterator it = M_sirm.begin(), end = M_sirm.end();
          it != end;
          ++it )
    {
        //
        // TODO: add it->calculateOutput() to get actual value and update SIRM::M_membership
        //

        it->trainSIRM( target, actual );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DefensiveSIRMsModel::specifyDomain( const int index_attribute,
                           const double min_domain,
                           const double max_domain )
{
    M_sirm[index_attribute].setDomain( min_domain, max_domain );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefensiveSIRMsModel::saveParameters( const std::string & prefix )
{
    for ( std::vector< DefensiveSIRM >::iterator it = M_sirm.begin(), end = M_sirm.end();
          it != end;
          ++it )
    {
        if ( ! it->saveParameters( prefix ) )
        {
            return false;
        }
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DefensiveSIRMsModel::loadParameters( const std::string & prefix )
{
    for ( std::vector< DefensiveSIRM >::iterator it = M_sirm.begin(), end = M_sirm.end();
          it != end;
          ++it )
    {
        if ( ! it->loadParameters( prefix ) )
        {
            return false;
        }
    }

    //std::cout << prefix << std::endl;

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */

/*-------------------------------------------------------------------*/
/*!

 */
