
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

void usage()
{
    std::cerr << "dlog_rank_extract FILES..." << std::endl;
}

void
extract_rank_data( std::istream & is,
                   std::ostream & os )
{
    std::string line;
    int n_line = 0;

    while ( std::getline( is, line ) )
    {
        int cycle, stopped;
        int level;
        char type;
        int n_read = 0;

        const char * buf = line.c_str();

        ++n_line;
        if ( std::sscanf( buf, "%d,%d %d %c %n",
                          &cycle, &stopped, &level, &type,
                          &n_read ) != 4 )
        {
            std::cerr << n_line << ": Illegal Line ["
                      << line << "]" << std::endl;
            continue;
        }
        buf += n_read;

        if ( buf[0] == '('
             && ! std::strncmp( buf, "(rank) ", 7 ) )
        {
            os << buf + 7 << '\n';
        }
    }
}

int
main( int argc, char **argv )
{
    for ( int i = 1; i < argc; ++i )
    {
        std::string infile = argv[i];
        std::string outfile = infile + ".rank";

        std::ifstream fin( infile.c_str() );
        if ( ! fin.is_open() )
        {
            std::cerr << "ERROR: Could not open the file ["
                      << infile << "]" << std::endl;
            continue;
        }

        std::ofstream fout( outfile.c_str() );
        if ( ! fout.is_open() )
        {
            std::cerr << "ERROR: Could not open the file ["
                      << outfile << "]" << std::endl;
            continue;
        }

        extract_rank_data( fin, fout );
    }

    return 0;
}
