
#include <rcsc/common/logger.h>

#include <svmrank/svm_struct_api.h>

#include <boost/cstdint.hpp>

#include <vector>
#include <iostream>
#include <fstream>
#include <cstdio>

class Evaluator {
private:

    svmrank::STRUCTMODEL M_model;
    svmrank::STRUCT_LEARN_PARM M_learn_param;

    Evaluator()
      { }

public:

    explicit
    Evaluator( const std::string & modelfile );

    ~Evaluator();

    bool isValid() const
      {
          return M_model.svm_model != NULL;
      }

    double evaluate( const std::vector< double > & features );

};


class DebugLogProcessor {
private:
    Evaluator * M_evaluator;

    DebugLogProcessor()
        : M_evaluator( static_cast< Evaluator * >( 0 ) )
      { }
public:

    DebugLogProcessor( Evaluator * evaluator )
        : M_evaluator( evaluator )
      { }

    bool process( const std::string & infile,
                  const std::string & outfile );

private:


    bool processFile( std::ifstream & in,
                      std::ofstream & out );

    double evaluateSequence( const std::string & rank_data );

    void printSequence( std::ostream & out,
                        const int cycle,
                        const int stopped,
                        const int sequence_id,
                        const double value,
                        const std::vector< std::string > & sequence_log,
                        const std::string & rank_data );

};

/*-------------------------------------------------------------------*/
/*!

 */
Evaluator::Evaluator( const std::string & modelfile )
{
    M_model = svmrank::read_struct_model( modelfile.c_str(), &M_learn_param );
    if ( M_model.svm_model == NULL )
    {
        std::cerr << "ERROR: failed to read svmrank model file ["
                  << modelfile << "]" << std::endl;
        return;
    }

    if ( M_model.svm_model->kernel_parm.kernel_type == svmrank::LINEAR )
    {
        // Linear Kernel: compute weight vector
        svmrank::add_weight_vector_to_linear_model( M_model.svm_model );
        M_model.w = M_model.svm_model->lin_weights;
    }

}

/*-------------------------------------------------------------------*/
/*!

 */
Evaluator::~Evaluator()
{
    if ( M_model.svm_model != NULL )
    {
        svmrank::free_struct_model( M_model );
    }
}

/*-------------------------------------------------------------------*/
/*!

 */
double
Evaluator::evaluate( const std::vector< double > & features )
{
    svmrank::WORD * words = new svmrank::WORD[features.size() + 1];
    for ( size_t i = 0; i < features.size(); ++i )
    {
        words[i].wnum = (int32_t)(i + 1);
        words[i].weight = (float)features[i];
    }
    words[features.size()].wnum = 0;
    words[features.size()].weight = 0;

    svmrank::DOC doc;
    doc.docnum = 1;
    doc.queryid = 1;
    doc.costfactor = 1.0;
    doc.slackid = 0;
    doc.kernelid = -1;
    doc.fvec = svmrank::create_svector( words, NULL, 1.0 );

    delete [] words;

    if ( ! doc.fvec )
    {
        return 0.0;
    }

    double value = svmrank::classify_example( M_model.svm_model, &doc );

    svmrank::free_svector( doc.fvec );

    return value;
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DebugLogProcessor::process( const std::string & infile,
                            const std::string & outfile )
{
    if ( ! M_evaluator
         || ! M_evaluator->isValid() )
    {
        return false;
    }

    if ( infile.empty() )
    {
        std::cerr << "Error: Empty input filename" << std::endl;
        return false;
    }
    if ( outfile.empty() )
    {
        std::cerr << "Error: Empty output filename" << std::endl;
        return false;
    }

    std::ifstream in( infile.c_str() );
    std::ofstream out( outfile.c_str() );

    if ( ! in.is_open() )
    {
        std::cerr << "Error: Could not open the input file [" << infile << "]" << std::endl;
        return false;
    }
    if ( ! out.is_open() )
    {
        std::cerr << "Error: Could not open the input file [" << infile << "]" << std::endl;
        return false;
    }

    return processFile( in, out );
}

/*-------------------------------------------------------------------*/
/*!

 */
bool
DebugLogProcessor::processFile( std::ifstream & in,
                                std::ofstream & out )
{
    std::string line;
    int n_line = 0;

    int sequence_id = 0;
    std::vector< std::string > sequence_log;
    std::string rank_data;

    int rank_cycle = 0, rank_stopped = 0;

    while ( std::getline( in, line ) )
    {
        ++n_line;

        const char * buf = line.c_str();

        int n_read = 0;

        int cycle, stopped;
        boost::int32_t level = 0;
        char type;

        if ( std::sscanf( buf, "%d,%d %d %c %n",
                          &cycle, &stopped, &level, &type,
                          &n_read ) != 4
             || n_read == 0 )
        {
            std::cerr << __FILE__ " (processFile) "
                      << n_line << ": Illegal format [" << line << "]" << std::endl;
            return false;
        }

        if ( level < 0
             || ! ( level & rcsc::Logger::PLAN ) )
        {
            if ( ! rank_data.empty() )
            {
                double new_value = evaluateSequence( rank_data );
                printSequence( out, rank_cycle, rank_stopped,
                               sequence_id, new_value, sequence_log, rank_data );
            }
            else if ( ! sequence_log.empty() )
            {
                for ( std::vector< std::string >::const_iterator it = sequence_log.begin(), end = sequence_log.end();
                      it != end;
                      ++it )
                {
                    out << *it << '\n';
                }
            }

            rank_data.clear();
            sequence_log.clear();

            out << line << '\n';
            continue;
        }

        buf += n_read;

        int id = 0;
        double value = 0.0;

        if ( std::sscanf( buf, "%d: evaluation=%lf", &id, &value ) == 2 )
        {
            if ( ! rank_data.empty() )
            {
                double new_value = evaluateSequence( rank_data );
                printSequence( out, rank_cycle, rank_stopped,
                               sequence_id, new_value, sequence_log, rank_data );
            }

            sequence_id = id;
            sequence_log.clear();
            rank_data.clear();
        }
        else if ( buf[0] == '('
                  && ! std::strncmp( buf, "(rank) ", 7 )
                  && buf[7] != '#' )
        {
            rank_cycle = cycle;
            rank_stopped = stopped;
            rank_data = buf + 7;
        }
        else
        {
            sequence_log.push_back( line );
        }
    }

    if ( ! rank_data.empty() )
    {
        double new_value = evaluateSequence( rank_data );
        printSequence( out, rank_cycle, rank_stopped,
                       sequence_id, new_value, sequence_log, rank_data );
        rank_data.clear();
    }

    return true;
}

/*-------------------------------------------------------------------*/
/*!

 */
double
DebugLogProcessor::evaluateSequence( const std::string & rank_data )
{
    const char * buf = rank_data.c_str();
    int n_read = 0;

    double old_value = 0.0;
    char qid[64];

    if ( std::sscanf( buf, "%lf qid:%[^ ] %n", &old_value, qid, &n_read ) != 2
         || n_read == 0 )
    {
        std::cerr << "(evaluateSequence) illegal format [" << rank_data << "]"
                  << std::endl;
        return 0.0;
    }
    buf += n_read;
    n_read = 0;

    // read feature vector

    std::vector< double > features;
    int fid;
    double fval;
    while ( *buf != '\0'
            && std::sscanf( buf, " %d:%lf %n", &fid, &fval, &n_read ) == 2 )
    {
        features.push_back( fval );
        buf += n_read;
        n_read = 0;
    }

    if ( features.empty() )
    {
        std::cerr << "(evaluateSequence) no feature [" << rank_data << "]"
                  << std::endl;
        return 0.0;
    }

    return M_evaluator->evaluate( features );
}

/*-------------------------------------------------------------------*/
/*!

 */
void
DebugLogProcessor::printSequence( std::ostream & out,
                                  const int cycle,
                                  const int stopped,
                                  const int sequence_id,
                                  const double value,
                                  const std::vector< std::string > & sequence_log,
                                  const std::string & rank_data )
{
    out << cycle << ',' << stopped << ' ' << rcsc::Logger::PLAN << " M "
        << sequence_id << ": evaluation=" << value << '\n';

    for ( std::vector< std::string >::const_iterator it = sequence_log.begin(), end = sequence_log.end();
          it != end;
          ++it )
    {
        out << *it << '\n';

    }

    {
        const char * buf = rank_data.c_str();
        int n_read = 0;
        double old_value = 0.0;
        char qid[16];

        if ( std::sscanf( buf, "%lf qid:%[^ ] %n", &old_value, qid, &n_read ) != 2
             || n_read == 0 )
        {
            std::cerr << "(evaluateSequence) illegal format [" << rank_data << "]"
                      << std::endl;
        }

        out << cycle << ',' << stopped << ' ' << rcsc::Logger::PLAN << " M "
            << "(rank) " << value << " qid:" << qid << ' ' << (buf + n_read) << '\n';
    }

    out.flush();
}

/*-------------------------------------------------------------------*/
/*!

 */
int
main( int argc, char **argv )
{
    if ( argc < 3 )
    {
        std::cerr << "dlog_evaluator_svmrank MODEL LOG [LOG...]" << std::endl;
        return 1;
    }

    std::string modelfile = argv[1];

    Evaluator evaluator( modelfile );
    DebugLogProcessor processor( &evaluator );

    for ( int i = 2; i < argc; ++i )
    {
        std::string infile = argv[i];
        std::string outfile = infile + ".tmp";

        if ( processor.process( infile, outfile ) )
        {
            std::string oldfile = infile + ".old";
            std::rename( infile.c_str(), oldfile.c_str() );
            std::rename( outfile.c_str(), infile.c_str() );
        }
    }

    return 0;
}
