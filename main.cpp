#include <iostream>
#include <unordered_map>
#include <list>
#include <set>
#include <vector>
#include <sstream>
#include <map>
#include <algorithm>

using namespace std;

void usage()
{
	cout << "netwalker <width> <height> <puzzle>" << endl;
}

enum PieceType
{
	PT_EMPTY,
	PT_DEADEND,
	PT_LINE,
	PT_ELBOW,
	PT_T,
};

enum Direction
{
	D_N,
	D_E,
	D_S,
	D_W,
};

constexpr pair<int, int> displacement(Direction dir)
{
	if (dir == D_N)
		return { -1, 0 };
	else if (dir == D_E)
		return { 0, 1 };
	else if (dir == D_S)
		return { 1, 0 };
	/* D_W */
	return { 0, -1 };
}

Direction oppositeDirection(Direction d)
{
	return (Direction)((d + 2) % 4);
}

using Piece = array<bool, 4>;

struct UnsolvableException : public runtime_error
{
	UnsolvableException()
		: runtime_error("Unsolvable puzzle") {}
};

struct Cell
{
	PieceType type;

	set<Piece> candidates;
	array<unsigned, 4> stakes = {};

	void refreshStakes()
	{
		stakes = {};
		for (auto c : candidates)
		{
			for (int i = 0; i < 4; i++)
			{
				if (c[i])
					stakes[i]++;
			}
		}
	}

	Cell(PieceType type)
		: type(type)
	{
		Piece candidate;

		switch (type)
		{
		case PT_EMPTY:
			candidate = { false, false, false, false };
			break;
		case PT_DEADEND:
			candidate = { true,  false, false, false };
			break;
		case PT_LINE:
			candidate = { true,  false, true,  false };
			break;
		case PT_ELBOW:
			candidate = { true,  true,  false, false };
			break;
		case PT_T:
			candidate = { true,  true,  true,  false };
			break;
		}

		for (int i = 0; i < 4; i++)
		{
			candidates.insert(candidate);
			Piece newCandidate;
			for (int j = 0; j < 4; j++)
				newCandidate[(j + 1) % 4] = candidate[j];
			candidate = newCandidate;
		}

		refreshStakes();
	}

	bool isSolved()
	{
		return candidates.size() == 1;
	}
};

static PieceType charToPT(char c)
{
	if (c >= 'a') /* hex >= 0xA */
		c = c - 'a' + 10;
	else if (c >= 'A') /* source */
		c -= 'A';
	else /* hex < 0xA */
		c -= '0';

	if (c == 0)
		return PT_EMPTY;

	while (c % 2 == 0)
		c /= 2;

	if (c == 1)
		return PT_DEADEND;

	if (c == 5)
		return PT_LINE;

	int ones = 0;
	while (c)
	{
		if (c % 2)
			ones++;
		c /= 2;
	}

	if (ones == 2)
		return PT_ELBOW;

	if (ones == 3)
		return PT_T;

	throw runtime_error("Bad character found");
}

struct Table
{
	unsigned width;
	unsigned height;

	vector<vector<Cell>> cells;

	set<pair<int, int>> nonEmptyCells;

	set<pair<int, int>> unsolvedCells;

	Table(unsigned width, unsigned height, const string rawTable)
		: width(width), height(height), cells(height)
	{
		if (rawTable.length() != width * height)
			throw runtime_error("Bad raw puzzle length");

		for (unsigned i = 0; i < height; i++)
		{
			cells[i].reserve(width);
			for (unsigned j = 0; j < width; j++)
			{
				int idx = j + i * width;

				cells[i].push_back(charToPT(rawTable[idx]));

				if (cells[i].back().type != PT_EMPTY)
					nonEmptyCells.insert({ i, j });
			}
		}
		unsolvedCells = nonEmptyCells;

		for (unsigned i = 0; i < height; i++)
		{
			for (unsigned j = 0; j < width; j++)
				enforceStakes(i, j);
		}
		checkReachability();
	}

	bool enforceStakes(int line, int col)
	{
		line = (line + height) % height;
		col = (col + width) % width;

		Cell &cell = cells[line][col];

		bool ret = false;

		for (int i = 0; i < 4; i++)
		{
			auto disp = displacement((Direction)i);

			if (cell.stakes[i] == 0)
				ret |= closeBorder(line + disp.first, col + disp.second, oppositeDirection((Direction)i));
			else if (cell.stakes[i] == cell.candidates.size())
				ret |= openBorder(line + disp.first, col + disp.second, oppositeDirection((Direction)i));
		}

		return ret;
	}

	void checkReachability()
	{
		set<pair<int, int>> unvisited = nonEmptyCells;
		list<pair<int, int>> toVisit;

		toVisit.push_back(*unvisited.begin());
		unvisited.erase(toVisit.front());

		while (!toVisit.empty())
		{
			auto coords = toVisit.front();
			toVisit.pop_front();
			Cell &visited = cells[coords.first][coords.second];

			for (int i = 0; i < 4; i++)
			{
				if (!visited.stakes[i])
					continue;

				auto displ = displacement((Direction)i);

				unsigned line = (coords.first + height + displ.first) % height;
				unsigned col = (coords.second + width + displ.second) % width;

				auto it = unvisited.find({ line, col });
				if (it == unvisited.end())
					continue;

				unvisited.erase(it);
				toVisit.push_back({ line, col });
			}
		}

		if (!unvisited.empty())
			throw UnsolvableException();
	}

	bool closeBorder(int line, int col, Direction dir)
	{
		return forceBorder(line, col, dir, true);
	}

	bool openBorder(int line, int col, Direction dir)
	{
		return forceBorder(line, col, dir, false);
	}

	bool forceBorder(int line, int col, Direction dir, bool close)
	{
		line = (line + height) % height;
		col = (col + width) % width;

		Cell &cell = cells[line][col];

		if (cell.stakes[dir] == 0)
		{
			if (close)
				return false;
			throw UnsolvableException();
		}

		if (cell.stakes[dir] == cell.candidates.size())
		{
			if (!close)
				return false;
			throw UnsolvableException();
		}

		auto predicate = [&](bool wantsOpen)
		{
			if (close)
				return !wantsOpen;
			return wantsOpen;
		};

		set<Piece> newCandidates;
		for (auto c : cell.candidates)
		{
			if (predicate(c[dir]))
				newCandidates.insert(c);
		}

		cell.candidates = newCandidates;
		cell.refreshStakes();
		if (cell.isSolved())
			unsolvedCells.erase({ line, col });

		return enforceStakes(line, col);
	}

	template<typename T, typename U, typename V>
	bool attempt(int depth, T condition, U trial, V resolution);

	void solve(int maxDepth = std::numeric_limits<int>::max());
};

void Table::solve(int maxDepth)
{
again:
	for (int depth = 1; depth <= maxDepth; depth++)
	{
		bool nothingToDo = true;
		for (unsigned i = 0; i < height; i++)
		{
			for (unsigned j = 0; j < width; j++)
			{
				Cell &cell = cells[i][j];
				if (cell.isSolved())
					continue;

				nothingToDo = false;

				auto attemptToTamper = [&](Direction dir, bool close)
				{
					return attempt(depth, [&]()
					{
						return cell.stakes[dir] > 0 && cell.stakes[dir] < cell.candidates.size();
					},
					[&](Table &t)
					{
						return t.forceBorder(i, j, dir, close);
					},
					[&]()
					{
						return forceBorder(i, j, dir, !close);
					});
				};

				for (auto d : {D_W, D_S})
				{
					for (bool close : { true, false })
					{
						if (attemptToTamper(d, close))
							goto again;
					}
				}
			}
		}
		if (nothingToDo)
			throw *this;
	}
}

template<typename T, typename U, typename V>
bool Table::attempt(int depth,  T condition, U trial, V resolution)
{
	if (!condition())
		return false;

	try
	{
		Table other = *this;
		if (trial(other))
			other.checkReachability();
		other.solve(depth - 1);
	}
	catch (UnsolvableException &ex)
	{
		if (resolution())
			checkReachability();
		return true;
	}

	return false;
}

static char encodeCell(const Cell &cell, Direction d)
{
	if (cell.stakes[d] == 0)
		return ' ';
	if (cell.stakes[d] != cell.candidates.size())
		return '?';
	if (d % 2 == 0)
		return '|';
	return '-';
}

int main(int argc, char **argv)
{
	if (argc != 4)
		usage();

	Table table(stoi(argv[1]), stoi(argv[2]), argv[3]);

	try
	{
		table.solve();
	}
	catch (Table &solved)
	{
		table = solved;
	}

	for (auto &line : table.cells)
	{
		for (auto &cell : line)
			cout << " " << encodeCell(cell, D_N) << " ";
		cout << endl;
		for (auto &cell : line)
			cout << encodeCell(cell, D_W) << "+" << encodeCell(cell, D_E);
		cout << endl;
		for (auto &cell : line)
			cout << " " << encodeCell(cell, D_S) << " ";
		cout << endl;
	}

	return 0;
}
