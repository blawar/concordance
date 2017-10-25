#include "boost/thread.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>

struct RRS // Round Robin Stack, to minimize malloc calls and locks
{
	RRS(unsigned long maxSize)
	{
		m_end = 0;
		m_index = 0;
		m_maxSize = maxSize;
		m_buffer.resize(maxSize);
	}

	void push(std::pair<std::string, unsigned long> value)
	{
		while(size() == m_maxSize)
		{
			boost::this_thread::sleep(boost::posix_time::milliseconds(10));
		}

		m_buffer[m_end] = value;
		m_end = ++m_end % m_maxSize;
	}

	std::pair<std::string, unsigned long> pop()
	{
		if(!size())
		{
			throw std::runtime_error("cannot pop empty stack");
		}
		else
		{
			std::pair<std::string, unsigned long> swap = m_buffer[m_index];
			m_index = ++m_index % m_maxSize;
			return swap;
		}
	}

	unsigned long size()
	{
		if(m_end < m_index) return m_maxSize - m_end;
		return m_index - m_end;
	}

	std::vector< std::pair<std::string, unsigned long> > m_buffer;
	unsigned long m_end;
	unsigned long m_maxSize;
	unsigned long m_index;
};

struct Item
{
	Item() : m_count(0)
	{
	}
	const unsigned long& count() const { return m_count; }
	unsigned long& count() { return m_count; }
	const std::vector<unsigned long>& lines() const { return m_lines; }
	std::vector<unsigned long>& lines() { return m_lines; }

	unsigned long m_count;
	std::vector<unsigned long> m_lines;
};

class Concordance
{
	std::vector<std::map< std::string, Item> > m_maps;
	unsigned long m_threads;
	std::vector<boost::thread*> m_threadHandles;
	std::vector<RRS> m_stacks;
	unsigned long m_dispatchIndex;
	bool m_run;
	boost::shared_mutex m_access;


	std::string& file_get_contents(const char* pszFile, std::string& buffer)
	{
		buffer.erase();
		FILE* f = fopen(pszFile, "rb");
		if(!f) return buffer;

		fseek(f, 0, SEEK_END);

		long sz = ftell(f);

		if(sz == -1)
		{
			fclose(f);
			return buffer;
		}

		rewind(f);


		buffer.resize(sz);

		long bytes = fread((char*)buffer.c_str(), 1, sz, f);
		fclose(f);

		return buffer;
	}

	bool tokenize(const char* pszStr)
	{
		unsigned long currentSentence = 1;
		std::string token;

		for(unsigned long i = 0; pszStr[i]; i++)
		{
			char c = tolower(pszStr[i]);
			if(c >= 'a' && c <= 'z')
			{
				token += tolower(pszStr[i]);
				continue;
			}

			if(c == '.')
			{
				if (token.length()) dispatch(token, currentSentence);
			}

			switch(c)
			{
				case ' ': case '\t': case '\n': case '\r':
					if (token.length())
					{
						if (token.length())
						{
							dispatch(token, currentSentence);
						}
						token = "";
					}
					break;
				case '.': case '?': case '!':
					currentSentence++;
					if (token.length())
					{
						dispatch(token, currentSentence);
					}
					token = c;
					dispatch(token, currentSentence);
					token = "";
					break;
			}
		}
		if(token.size()) dispatch(token, currentSentence);

		return true;
	}

	void dispatch(std::string& s, unsigned long currentSentence)
	{
		m_stacks[m_dispatchIndex].push(std::pair<std::string, unsigned long>(s, currentSentence));
		m_dispatchIndex = ++m_dispatchIndex % threads();
	}

	bool createWorker(unsigned long id)
	{
		RRS emptyStack(50000);
		m_stacks.push_back(emptyStack);

		std::map< std::string, Item> m;
		m_maps.push_back(m);
		m_threadHandles.push_back(new boost::thread(boost::bind(&Concordance::worker, this, id)));
	}

public:

	void worker(unsigned long id)
	{
		std::pair<std::string, unsigned long> p;
		while(m_run || m_stacks[id].size())
		{
			if(!m_stacks[id].size())
			{
				boost::this_thread::sleep(boost::posix_time::milliseconds(10));
				continue;
			}

			p = m_stacks[id].pop();

			if(p.first.size() < 4) continue;

                       	m_maps[id][p.first].lines().push_back(p.second);
                       	m_maps[id][p.first].count()++;
		}
	}

	Concordance() : m_threads(4), m_dispatchIndex(0), m_run(true)
	{
	}

	bool loadFile(const char* pszFile)
        {
                std::string buffer;

                file_get_contents(pszFile, buffer);

		if(!buffer.size())
		{
			return false;
		}

		tokenize(buffer.c_str());
		return true;
        }

	void createWorkers()
	{
		for(unsigned long i=0; i < threads(); i++)
		{
			createWorker(i);
		}
	}

	void joinWorkers()
	{
		m_run = false;

		for(std::vector<boost::thread*>::const_iterator it = m_threadHandles.begin(); it != m_threadHandles.end(); it++)
		{
			(*it)->join();
		}
	}

	void print()
	{
		std::map<std::string, Item> mergedMap;
		for(unsigned long i=0; i < m_maps.size(); i++)
		{
			for(std::map<std::string, Item>::const_iterator it = m_maps[i].begin(); it != m_maps[i].end(); it++)
			{
				mergedMap[it->first].lines().insert(mergedMap[it->first].lines().end(), (*it).second.lines().begin(), (*it).second.lines().end());
				mergedMap[it->first].count() += (*it).second.count();
			}
		}
		for(std::map<std::string, Item>::const_iterator it = mergedMap.begin(); it != mergedMap.end(); it++)
		{
			const Item& item = it->second;
			const std::string& word = it->first;

			std::cout << word<< "\t{" << item.count() << ":";

			for(unsigned long i=0; i < item.lines().size(); i++)
			{
				if(i) std::cout << ",";
				std::cout << item.lines()[i];
			}
			std::cout << "}" << std::endl;
		}
	}

	unsigned long& threads() { return m_threads; }
};

int main(int argc, char** argv)
{
	Concordance con;
	con.threads() = 1;
	char* pszFile = NULL;

	for(unsigned int i=1; i < argc; i++)
	{
		if(*argv[i] == '-')
		{
			switch(argv[i][1])
			{
				case 't':
					if(++i < argc)
					{
						con.threads() = atol(argv[i]);
					}
					break;
			}
		}
		else
		{
			pszFile = argv[i];
		}
	}

	if(!pszFile)
	{
		std::cout << "must specify input file" << std::endl;
		return -1;
	}

	con.createWorkers();
	con.loadFile(pszFile);
	con.joinWorkers();

	con.print();
}
