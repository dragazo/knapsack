// author:      Cruz Jean
// course:      CSCI6100-001
// project:     OLA3 (0/1 knapsack)
// description: implement several algorithms to solve the 0/1 knapsack problem and test their performance.
//              required algorithms:
//              1. dynamic programming
//              2. backtracking
//              3. branch and bound
// inputs:      nothing. no input is taken from the user. all values are either built-in or randomly generated.
// outputs:     a tabularized summary of execution performance for each algorithm working on the same inputs.
//              performance is measured by both number of value comparisons and elapsed runtime.
//              due to how different the algorithms are from one another, the elapsed runtime is the primary statistic used in experimental analysis.
//              output includes the number of items under consideration, the size of the knapsack, and the random ranges of the value and weights of items.
//              for each pass, random items are generated and each algorithm is run on the same input items and knapsack size.
//              in addition to the relevant statistics, the actual solutions are also printed for each pass.

#include <iostream>
#include <iomanip>
#include <vector>
#include <utility>
#include <algorithm>
#include <numeric>
#include <queue>
#include <random>
#include <chrono>

struct item_t
{
	std::size_t weight; // the weight of this item
	std::size_t value;  // the value of this item
};
struct solution_t
{
	std::vector<std::size_t> items; // the collection of items in this solution (index)
	std::size_t total_weight = 0;   // the total weight of this solution
	std::size_t total_value = 0;    // the total value of this solution
	std::size_t comparisons = 0;    // total number of comparisons made
};

// == comparison for solutions only compares total value
bool operator==(const solution_t &a, const solution_t &b) { return a.total_value == b.total_value; }
bool operator!=(const solution_t &a, const solution_t &b) { return !(a == b); }

std::ostream &operator<<(std::ostream &ostr, const solution_t &s)
{
	ostr << "value: " << s.total_value << " weight: " << s.total_weight << " [" << std::setw(8) << s.comparisons << " cmp] ";
	for (std::size_t i = 0; i < s.items.size() && i < 10; ++i) ostr << i << ' ';
	if (s.items.size() > 10) ostr << "...";
	return ostr;
}

template<typename T>
class matrix
{
private:
	std::vector<T> d;
	std::size_t _cols;
public:
	matrix(std::size_t rows, std::size_t cols) : d(rows * cols), _cols(cols) {}

	T &operator()(std::size_t row, std::size_t col) { return d[row * _cols + col]; }
	const T &operator()(std::size_t row, std::size_t col) const { return d[row * _cols + col]; }

	friend std::ostream &operator<<(std::ostream &ostr, const matrix<T> &m)
	{
		for (std::size_t i = 0; i < m.d.size() / m._cols; ++i)
		{
			for (std::size_t j = 0; j < m._cols; ++j) ostr << std::setw(4) << m(i, j) << ' ';
			ostr << '\n';
		}
		return ostr;
	}
};

// solves knapsack problem using dynamic programming algorithm
solution_t dynamic(const std::vector<item_t> &items, std::size_t max_weight)
{
	if (items.empty() || max_weight <= 0) return {}; // if there are no items or no max weight, return no empty solution

	matrix<std::size_t> table(items.size(), max_weight + 1); // allocate the entire table - flattened array
	std::vector<std::size_t> best; // the best solution (calculated at end)
	std::size_t comparisons = 0; // initialize number of comparisons made

	// initialize the first row (trivial)
	for (std::size_t i = 0; i < items[0].weight && i <= max_weight; ++i) table(0, i) = 0;
	for (std::size_t i = items[0].weight; i <= max_weight; ++i) table(0, i) = items[0].value;

	// generate following rows based on the previous
	for (std::size_t row = 1; row < items.size(); ++row)
	{
		// for each weight limit before this item, copy from previous row
		for (std::size_t w = 0; w < items[row].weight && w <= max_weight; ++w)
		{
			table(row, w) = table(row - 1, w);
		}
		// after that, take max of above (not take) and above to left (take)
		for (std::size_t w = items[row].weight; w <= max_weight; ++w)
		{
			std::size_t take = table(row - 1, w - items[row].weight) + items[row].value;
			std::size_t not_take = table(row - 1, w);

			table(row, w) = take > not_take ? take : not_take;
			++comparisons; // add one comparison for the max operation
		}
	}

	// compute the set of items we took
	std::size_t w = max_weight;
	for (std::size_t i = items.size(); --i; )
	{
		if (table(i, w) != table(i - 1, w)) { best.push_back(i); w -= items[i].weight; }
	}
	if (table(0, w) != 0) { best.push_back(0); w -= items[0].weight; }
	comparisons += items.size(); // add 1 comparison for each above !=

	// reverse best list set to be in ascending index order
	std::reverse(best.begin(), best.end());
	return { std::move(best), max_weight - w, table(items.size() - 1, max_weight), comparisons };
}

struct backtracking_node
{
	std::size_t depth = 0;          // depth of this node in the tree
	std::vector<std::size_t> items; // the list of items taken
	std::size_t weight = 0;         // total amount of weight of all items
	std::size_t value = 0;          // total value of all items

	friend bool operator<(const backtracking_node &a, const backtracking_node &b) { return a.value > b.value; } // sort by value descending
};

// solves knapsack problem using backtracking algorithm
solution_t backtracking(const std::vector<item_t> &items, std::size_t max_weight)
{
	std::priority_queue<backtracking_node> queue;
	std::size_t comparisons = 0; // initialize number of comparisons made

	backtracking_node best; // best is trivial empty node
	queue.emplace(); // add a trivial node to the queue

	// while we still have stuff in the queue
	while (!queue.empty())
	{
		// remove the top item
		auto top = queue.top();
		queue.pop();

		// if this node is not terminal, create children for it
		if (top.depth < items.size())
		{
			// add the left child (not taking this item)
			backtracking_node n = top;
			n.depth++;
			queue.emplace(std::move(n));

			// if we have room for this item, also add right child (taking this item)
			if (top.weight + items[top.depth].weight <= max_weight)
			{
				n = top;
				n.depth++;
				n.weight += items[top.depth].weight;
				n.value += items[top.depth].value;
				n.items.push_back(top.depth);
				queue.emplace(std::move(n));
			}
		}
		// otherwise this is terminal - if better than best, use it
		else
		{
			if (top.value > best.value) best = top;
			++comparisons;
		}
	}

	return { std::move(best.items), best.weight, best.value, comparisons };
}

struct branchbound_node
{
	std::size_t depth = 0;          // depth of this node in the tree
	std::vector<std::size_t> items; // the list of items taken
	std::size_t weight = 0;         // total amount of weight of all items
	std::size_t value = 0;          // total value of all items

	friend bool operator<(const branchbound_node &a, const branchbound_node &b) { return a.value > b.value; } // sort by value descending
};
struct branchbound_item
{
	item_t item;
	double value_density;
};

// calculates upper bound of value for the given node
double calc_upper_bound(const std::vector<branchbound_item> &densities, const branchbound_node &node, std::size_t max_weight)
{
	std::size_t w = node.weight; // initialize weight
	double value = node.value;   // initialize value

	// for each item this depth and onward
	for (std::size_t i = node.depth; i < densities.size(); ++i)
	{
		// if we have room for the entire item, take it all
		if (w + densities[i].item.weight <= max_weight)
		{
			w += densities[i].item.weight;
			value += densities[i].item.value;
		}
		// otherwise take a piece of it
		else
		{
			value += (max_weight - w) * densities[i].value_density;
			break;
		}
	}

	return value;
}

// solves knapsack problem using branch and bound algorithm
solution_t branchbound(const std::vector<item_t> &items, std::size_t max_weight)
{
	// calculate value densities (value per weight)
	std::vector<branchbound_item> densities(items.size());
	for (std::size_t i = 0; i < items.size(); ++i)
	{
		double density = (double)items[i].value / items[i].weight;
		densities[i] = {items[i], density};
	}
	// sort by value density descending
	std::sort(densities.begin(), densities.end(), [](const branchbound_item &a, const branchbound_item &b) {return a.value_density > b.value_density; });

	std::priority_queue<branchbound_node> queue;
	std::size_t comparisons = 0; // initialize total number of comparisons made

	// initialize best to trivial empty node and add an empty node to the queue
	branchbound_node best;
	queue.emplace(best);

	// while there's still something in the queue
	while (!queue.empty())
	{
		// remove it from the queue
		auto top = queue.top();
		queue.pop();

		// if this is non-terminal, explore it
		if (top.depth < items.size())
		{
			// add the left child (not taking this item)
			branchbound_node n = top;
			n.depth++;
			if (calc_upper_bound(densities, n, max_weight) > best.value) queue.emplace(std::move(n)); // only add to queue if promising
			++comparisons; // add 1 comparison for pruning operation

			// if we have room for this item, also add right child (taking this item)
			if (top.weight + items[top.depth].weight <= max_weight)
			{
				n = top;
				n.depth++;
				n.weight += items[top.depth].weight;
				n.value += items[top.depth].value;
				n.items.push_back(top.depth);
				if (calc_upper_bound(densities, n, max_weight) > best.value) queue.emplace(std::move(n)); // only add to queue if promising
				++comparisons; // add 1 comparison for pruning operation
			}
		}
		// otherwise this is terminal - if better than best, use it
		else
		{
			if (top.value > best.value) best = top;
			++comparisons; // add 1 comparison for the max operation
		}
	}

	return { std::move(best.items), best.weight, best.value, comparisons };
}

int main()
{
	std::mt19937 rand(std::random_device{}()); // initialize the random number generator
	std::vector<item_t> items; // the collection of items to use for each pass

	// the settings to use for each pass to generate random data
	struct
	{
		std::size_t items, weight, value, maxweight;
	} _settings[] = {
		{ 4, 100, 100, 100 },
		{ 8, 100, 100, 100 },
		{ 16, 100, 100, 100 },
		{ 32, 100, 100, 100 },
		{ 64, 100, 100, 100 },
	};

	// the collection of algorithms to run for each pass
	struct
	{
		const char *name;
		solution_t(*func)(const std::vector<item_t>&, std::size_t);
	} algorithms[] = { { "dynamic         ", dynamic }, { "backtracking    ", backtracking }, { "branch and bound", branchbound } };

	// for each pass
	for (const auto &settings : _settings)
	{
		items.clear();

		// fill the items list with random data
		for (std::size_t i = 0; i < settings.items; ++i)
			items.push_back({ rand() % settings.weight + 1, rand() % settings.value + 1 });

		// print out a header
		std::cout << "items: " << settings.items << " max weight: " << settings.maxweight
			<< " rand weight: " << settings.weight << " rand value: " << settings.value << '\n';

		// apply each algorithm
		for (const auto &algorithm : algorithms)
		{
			auto start = std::chrono::high_resolution_clock::now();
			solution_t solution = algorithm.func(items, settings.maxweight);
			auto stop = std::chrono::high_resolution_clock::now();

			// print out algorithm name, elapsed time, and the solution info
			std::cout << algorithm.name << " (elapsed " << std::setw(8) << std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count() << " ms) : "
				<< solution << std::endl;
		}
		std::cout << '\n';
	}

	std::cin.get();
	return 0;
}