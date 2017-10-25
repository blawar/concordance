#include <iostream>
#include <string>
#include <vector>
#include <map>

struct Item
{
	const unsigned long& count() const { return m_count; }
	unsigned long& count() { return m_count; }
	const std::vector<unsigned long>& lines() const { return m_lines; }
	std::vector<unsigned long>& lines() { return m_lines; }

	unsigned long m_count;
	std::vector<unsigned long> m_lines;
};

class Concordance
{
	std::map<std::string, Item> m_map;

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

	bool tokenize(const char* pszStr, std::vector<std::string> &tokens)
	{
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
				if (token.length()) tokens.push_back(token);
			}

			switch(c)
			{
				case ' ': case '\t': case '\n': case '\r':
					if (token.length())
					{
						if (token.length())
						{
							tokens.push_back(token);
						}
						token = "";
					}
					break;
				case '.': case '?': case '!':
					if (token.length())
					{
						tokens.push_back(token);
					}
					token = c;
					tokens.push_back(token);
					token = "";
					break;
			}
		}
		if(token.size()) tokens.push_back(token);

		return true;
	}

public:
	bool loadFile(const char* pszFile)
        {
                std::string buffer;
		std::vector<std::string> tokens;

                file_get_contents(pszFile, buffer);

		if(!buffer.size())
		{
			// error
			return false;
		}

		tokenize(buffer.c_str(), tokens);

		unsigned long currentLine = 1;

		for(std::vector<std::string>::const_iterator it = tokens.begin(); it != tokens.end(); it++)
		{
			if(!(*it).compare(".") || !(*it).compare("!") || !(*it).compare("?"))
			{
				currentLine++;
				continue;
			}

			if((*it).size() < 4) continue;

			m_map[*it].lines().push_back(currentLine);
			m_map[*it].count()++;
		}
		return true;
        }

	void print()
	{
		for(std::map<std::string, Item>::const_iterator it = m_map.begin(); it != m_map.end(); it++)
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
};

int main(int argc, char** argv)
{
	Concordance con;

	for(unsigned int i=1; i < argc; i++)
	{
		con.loadFile(argv[i]);
	}

	con.print();
}
