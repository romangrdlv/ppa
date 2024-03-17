#include <math.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <chrono>
#include "lib/json-develop/single_include/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

struct Project
{
    string name = "";
    int id = -1;
    int num = 0;
    int lifetime = 0;
    int inv = 0;
    float npv = 0;
    float risk = 0;
    vector<float> incomes;
    vector<float> expences;
};

int main(int argc, char *argv[])
{
    // parsing input string
    bool calc = true;

    double budget = -1;
    int max_lifetime  = __INT_MAX__;
    string file_name = string(argv[argc - 1]);
    string output_file_name = "";

    for (int arg = 0; arg < argc; arg++)
    {
    	string str_arg = string(argv[arg]);
    	if (str_arg.substr(0, 1) == "-")
    	{
    		if (str_arg == "-b" or str_arg == "-B" or str_arg == "--budget")
    		{
    			budget = stod(string(argv[arg + 1]));
    			arg++;
    		}
            if (str_arg == "-t" or str_arg == "-T" or str_arg == "--time")
    		{
    			max_lifetime = stoi(string(argv[arg + 1]));
    			arg++;
    		}
    		if (str_arg == "-o" or str_arg == "--output")
    		{
    			output_file_name = string(argv[arg + 1]);
    			arg++;
    		}
            if (str_arg == "-i" or str_arg == "--input")
    		{
    			file_name = string(argv[arg + 1]);
    			arg++;
    		}
    		if (str_arg == "-h" or str_arg == "--help")
    		{
    			calc = false;
    		}
    	}
    }

    // printing help
    if (argc == 0 or !calc)
    {
        cout << endl
             << "Usage: ./ppa [OPTIONS] [INPUT FILE]" << endl
             << endl
             << "Optimize a project portfolio" << endl
             << endl
             << "Options:" << endl
             << "-b, --budget <float>     set the budget" << endl
             << "-t, --time <int>         set the max project lifetime (default - unlimited)" << endl
             << "-o, --output <name>      set the output file name (default - prints to terminal)" << endl
             << "-h, --help               display this text and exit" << endl
             << endl
             << "NOTE: all float values (except risks) are rounded up to 2 digits" << endl;
        return 0;
    }

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

    // cheching inputs
    if (budget == -1)
    {
        std::cerr << "ERR: budget is not specified" << endl;
        return -1;
    }

    // parsing json
    std::ifstream input_file(file_name);
    string input_string;
    string line;
    if (input_file.is_open())
    {
        while (input_file.good())
        {
            std::getline(input_file, line);
            input_string += line;
        }
        input_file.close();
    }
    else
    {
        std::cerr << "ERR: file " + file_name + " not found" << endl;
        return -1;
    }
    if (input_string.empty())
    {
        std::cerr << "ERR: file " + file_name + " not found" << endl;
        return -1;
    }

    auto input_json = json::parse(input_string);
    vector<Project> projects;

    for (auto i : input_json["projects"])
    {
        projects.push_back(
            {
                .name = i["name"],
                .num = i["number"],
                .lifetime = i["lifetime"],
                .inv = i["inv"],
                .risk = i["risk"],
                .incomes = i["incomes"],
                .expences = i["expences"],
            });
    }

    int initial_projects_size = projects.size();
    // checking projects parameters
    for (int i = 0; i < projects.size(); i++)
    {
        if (!(projects[i].lifetime == projects[i].incomes.size() and projects[i].incomes.size() == projects[i].expences.size()))
        {
            std::cerr << "ERR: project \"" << projects[i].name << "\" has different sizes of arrays of incomes and expences or lifetime dosen't match to them (expected " << projects[i].lifetime << ", has " << projects[i].incomes.size() << " and " << projects[i].expences.size() << ")" << endl;
            return -1;
        }
    }

    // calculating npv & rounding up
    for (int i = 0; i < projects.size(); i++)
    {
        projects[i].inv = ceil(projects[i].inv * 100); // we have to multiply inv by 100 in order to use it in knapsack problem solving
        projects[i].lifetime = min(projects[i].lifetime, max_lifetime);
        for (int t = 0; t < projects[i].lifetime; t++)
        {
            projects[i].npv += (ceil(projects[i].incomes[t] * 100) / float(100) - ceil(projects[i].expences[t] * 100) / float(100)) / pow(1 + projects[i].risk, t);
        }
    }
    budget = ceil(budget * 100); // same reason

    // resolving number
    for (int i = 0; i < initial_projects_size; i++)
    {
        projects[i].id = i;
        if (projects[i].num > 1)
        {
            int copies = projects[i].num - 1;
            projects[i].num = 1;
            for (int k = 0; k < copies; k++)
            {
                projects.push_back(projects[i]);
            }
        }
    }

    // solving knapsack
    float mem[projects.size() + 1][int(budget + 1)];
    for (int i = 0; i <= budget; ++i)
    {
        mem[0][i] = 0;
    }
    for (int i = 1; i <= projects.size(); ++i)
    {
        for (int s = 0; s <= budget; ++s)
        {
            if (projects[i - 1].inv > s)
                mem[i][s] = mem[i - 1][s];
            else
            {
                mem[i][s] = max(mem[i - 1][s], mem[i - 1][int(s - projects[i - 1].inv)] + projects[i - 1].npv);
            }
        }
    }
    float revenue = mem[projects.size()][int(budget)];

    int x[projects.size()];
    int pos = budget;
    for (int i = 0; i < projects.size(); ++i)
    {
        x[i] = 0;
    }
    for (int i = projects.size(); i > 0; i--)
    {
        if (mem[i][pos] > mem[i - 1][pos])
        {
            x[i - 1] = 1;
            pos -= projects[i - 1].inv;
        }
    }

    budget /= 100;
    float unused = budget;
    for (int i = 0; i < projects.size(); i++)
    {
        projects[i].inv /= 100;
        if (x[i])
        {
            unused -= projects[i].inv;
        }
    }

    // collapsing
    int current_projects_size = projects.size();
    for (int i = initial_projects_size; i < current_projects_size; i++)
    {
        x[projects[i].id] += x[i];
        projects.erase(projects.begin() + i);
    }

    // print output
    cout << "MAX REVENUE: " << revenue;
    if (unused)
    {
        cout << " (+" << unused << " unused investments)";
    }
    cout << endl;
    cout << "PROJECTS:" << endl;
    for (int i = 0; i < initial_projects_size; i++)
    {
        if (x[i])
        {
            cout << projects[i].name;
            if (x[i] > 1)
            {
                cout << " (x" << x[i] << ")";
            }
            cout << endl;
        }
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    cout << "Time elapsed: " << float(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) / 1000 << " milliseconds" << endl;

    // write to file
    if (!output_file_name.empty())
    {
        vector<json> json_projects;
        for (int i = 0; i < projects.size(); i++)
        {
            json project;
            project["name"] = projects[i].name;
            project["used"] = x[i];
            project["lifetime"] = projects[i].lifetime;
            project["inv"] = projects[i].inv;
            project["npv"] = projects[i].npv;
            project["risk"] = projects[i].risk;
            project["incomes"] = projects[i].incomes;
            project["expences"] = projects[i].expences;

            json_projects.push_back(project);
        }

        json json_output;
        json_output["budget"] = budget;
        json_output["max_lifetime"] = max_lifetime;
        json_output["revenue"] = revenue;
        json_output["unused"] = unused;
        json_output["projects"] = json_projects;
        string string_output = json_output.dump(4, ' ');

        std::ofstream output_file(output_file_name);
        output_file << json_output;
        output_file.close();
    }
    return 0;
}