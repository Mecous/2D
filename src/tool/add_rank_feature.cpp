
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {
bool add_feature = true;
}

bool
process_file( const int feature_id,
              const std::string & infile,
              const std::string & outfile )
{
    std::ifstream fin( infile.c_str() );
    std::ofstream fout( outfile.c_str() );

    if ( ! fin.is_open() )
    {
        std::cerr << "ERROR: could not open the input file [" << infile
                  << "]" << std::endl;
        return false;
    }

    if ( ! fout.is_open() )
    {
        std::cerr << "ERROR: could not open the output file [" << outfile
                  << "]" << std::endl;
        return false;
    }

    std::string line;
    while ( std::getline( fin, line ) )
    {
        if ( line.empty() ) continue;
        if ( line[0] == '#' )
        {
            fout << line << '\n';
            continue;
        }

        double value;
        char qid[64];
        int fid;
        double fval;

        int fcount = 0;
        int n_read = 0;

        const char * buf = line.c_str();

        if ( std::sscanf( buf, " %lf qid:%[^ ] %n", &value, qid, &n_read ) != 2 )
        {
            std::cerr << "ERROR: illegal line [" << line << "]" << std::endl;
            return false;
        }
        buf += n_read;

        fout << value << " qid:" << qid;

        while ( std::sscanf( buf, " %d:%lf %n", &fid, &fval, &n_read ) == 2 )
        {
            ++fcount;
            buf += n_read;

            if ( fcount < feature_id )
            {
                fout << ' ' << fid << ':' << fval;
            }
            else if ( fcount == feature_id )
            {
                if ( add_feature )
                {
                    fout << ' ' << feature_id << ":0.0";
                    fout << ' ' << fid + 1 << ':' << fval;
                }
                // else
                // {
                //    remove feature
                // }
            }
            else
            {
                if ( add_feature )
                {
                    fout << ' ' << fid + 1 << ':' << fval;
                }
                else
                {
                    fout << ' ' << fid - 1 << ':' << fval;
                }
            }
        }

        if ( feature_id == fcount + 1 )
        {
            fout << ' ' << feature_id << ":0.0";
        }
        else if ( fcount < feature_id )
        {
            std::cerr << "ERROR: illegal feature count"
                      << " count=" << fcount << " new_id=" << feature_id
                      << " [" << line << "]"
                      << std::endl;
            return false;
        }
        fout << '\n';
    }

    fin.close();
    fout.close();

    return true;
}

void
rename_files( const std::string & infile,
              const std::string & outfile )
{
    std::string oldfile = infile + ".old";

    if ( std::rename( infile.c_str(), oldfile.c_str() ) != 0 )
    {
        std::cerr << "ERROR: could not rename the file [" << infile
                  << "] -> [" << oldfile << "]" << std::endl;
        return;
    }

    if ( std::rename( outfile.c_str(), infile.c_str() ) != 0 )
    {
        std::cerr << "ERROR: could not rename the file [" << outfile
                  << "] -> [" << infile << "]" << std::endl;
        return;
    }
}

void
usage()
{
    std::cerr << "add_rank_feature {-a|-r} FEATURE_INDEX FILENAME"
              << std::endl;
}


int
main( int argc, char ** argv )
{
    if ( argc < 4 )
    {
        usage();
        return 1;
    }

    if ( ! std::strcmp( "-a", argv[1] ) )
    {
        add_feature = true;
    }
    else if ( ! std::strcmp( "-r", argv[1] ) )
    {
        add_feature = false;
    }
    else
    {
        usage();
        return 1;
    }

    int feature_id = atoi( argv[2] );
    if ( feature_id <= 0 )
    {
        std::cerr << "ERROR: illegal feature_id " << feature_id
                  << std::endl;
        return 1;
    }

    std::string infile = argv[3];
    std::string outfile = infile + ".new";

    if ( add_feature )
    {
        std::cerr << "add feature " << feature_id << std::endl;
    }
    else
    {
        std::cerr << "remove feature " << feature_id << std::endl;;
    }

    if ( ! process_file( feature_id, infile, outfile ) )
    {
        return 1;
    }

    rename_files( infile, outfile );

    return 0;
}
