/* MBDyn UDE: genetic_algorithm
 *
 * Parses GA block, loads libpop/libfit/libcons via dlopen, runs GA once,
 * and exposes best genome via private data outputs.
 */

#include "mbconfig.h"

#include <dlfcn.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#include "dataman.h"
#include "userelem.h"

static inline double clamp01(double x) {
	return x < 0. ? 0. : (x > 1. ? 1. : x);
}

/* ===== Plugin API expected from the .so files ===== */
typedef void   (*gen_pop_fn_t)(double* population, int popSize, int genomeLen);
typedef double (*fitness_fn_t)(const double* genome, int genomeLen);
typedef int    (*constraint_fn_t)(const double* genome, int genomeLen);

/* ===== UDE implementation ===== */
class GeneticAlgorithm final : public UserDefinedElem {
public:
	struct InputRef {
		unsigned label;         // element label (e.g., 101, 102, …)
		std::string compExpr;   // e.g., "M[2]"
		bool direct;            // 'direct' keyword present
	};

private:
	/* parsed config */
	std::vector<InputRef> inputs_;     // N inputs => genome length = N
	std::vector<unsigned> outputs_;    // values we expose as priv-data (length can be <= genome length)

	std::string popLib_, consLib_;  // Only 2 libraries: GA and constraint
	int popSize_     = 20;
	int numGen_      = 1;
	double crossRate_= 0.9;
	double mutRate_  = 0.05;
	int elitism_     = 2;
	unsigned seed_   = 12345u;

	/* dynamic libs & function pointers */
	void* hPop_  = nullptr;  // GA library handle
	void* hCons_ = nullptr;  // constraint library handle
	gen_pop_fn_t      gen_pop_      = nullptr;
	fitness_fn_t      fitness_      = nullptr;
	constraint_fn_t   constraint_ok_= nullptr;

	/* GA state */
	std::vector<double> bestGenome_;  // length = genomeLen
	double bestFitness_ = -1e300;
	bool solved_ = false;

public:
	GeneticAlgorithm(unsigned uLabel, const DofOwner* pDO, DataManager* pDM, MBDynParser& HP)
	: UserDefinedElem(uLabel, pDO)
	{
		/* ---- Parse input block ----
		 * New format matches your specification:
		 * inputs number, N,
		 *   element, <id>, joint, string, "M[2]", direct,
		 *   ...
		 * genetic algorithm, "libga.so",
		 * constraint function, "libcons.so", 
		 * population, N,
		 * generations, N,
		 * outputs number, N,
		 *   node, <id>,
		 *   ...
		 */

		if (HP.IsKeyWord("help")) {
			silent_cout(
				"User Defined Element: genetic_algorithm\n"
				"Format:\n"
				"  inputs number, N,\n"
				"    element, <id>, joint, string, \"M[2]\", direct,\n"
				"    ...\n"
				"  genetic algorithm, \"libga.so\",\n"
				"  constraint function, \"libcons.so\",\n"
				"  population, N,\n"
				"  generations, N,\n"
				"  outputs number, N,\n"
				"    node, <id>,\n"
				"    ...\n"
			<< std::endl);
			if (!HP.IsArg()) throw NoErr(MBDYN_EXCEPT_ARGS);
		}

		/* inputs number, N, then N entries:
		 *   element, <label>, joint, string, "M[2]", direct,
		 */
		if (!HP.IsKeyWord("inputs")) {
			silent_cerr("genetic_algorithm: expected 'inputs' keyword" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		if (!HP.IsKeyWord("number")) {
			silent_cerr("genetic_algorithm: expected 'number' after 'inputs'" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		int nInputs = HP.GetInt();
		if (nInputs <= 0) {
			silent_cerr("genetic_algorithm: inputs number must be > 0" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		inputs_.reserve(nInputs);

		for (int i = 0; i < nInputs; ++i) {
			if (!HP.IsKeyWord("element")) {
				silent_cerr("genetic_algorithm: expected 'element' for input #" << (i+1) << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			unsigned lbl = static_cast<unsigned>(HP.GetInt());

			/* Parse: joint, string, "M[2]", direct */
			if (!HP.IsKeyWord("joint")) {
				silent_cerr("genetic_algorithm: expected 'joint' after element " << lbl << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			if (!HP.IsKeyWord("string")) {
				silent_cerr("genetic_algorithm: expected 'string' after element/joint " << lbl << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}

			std::string comp = HP.GetString(); /* e.g., "M[2]" */

			bool direct = false;
			if (HP.IsKeyWord("direct")) { 
				direct = true; 
			}

			inputs_.push_back({lbl, comp, direct});
		}

		/* genetic algorithm, "libga.so" */
		if (!HP.IsKeyWord("genetic")) {
			silent_cerr("genetic_algorithm: expected 'genetic' keyword" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		if (!HP.IsKeyWord("algorithm")) {
			silent_cerr("genetic_algorithm: expected 'algorithm' after 'genetic'" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		popLib_ = HP.GetString(); // e.g., "libga.so"

		/* constraint function, "libcons.so" */
		if (!HP.IsKeyWord("constraint")) {
			silent_cerr("genetic_algorithm: expected 'constraint' keyword" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		if (!HP.IsKeyWord("function")) {
			silent_cerr("genetic_algorithm: expected 'function' after 'constraint'" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		consLib_ = HP.GetString(); // e.g., "libcons.so"

		/* population, N */
		if (!HP.IsKeyWord("population")) {
			silent_cerr("genetic_algorithm: expected 'population' keyword" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		popSize_ = HP.GetInt();

		/* generations, N */
		if (!HP.IsKeyWord("generations")) {
			silent_cerr("genetic_algorithm: expected 'generations' keyword" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		numGen_ = HP.GetInt();

		/* outputs number, N, then N entries: node, <id>, */
		if (!HP.IsKeyWord("outputs")) {
			silent_cerr("genetic_algorithm: expected 'outputs' keyword" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		if (!HP.IsKeyWord("number")) {
			silent_cerr("genetic_algorithm: expected 'number' after 'outputs'" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		int nOut = HP.GetInt();
		if (nOut <= 0) {
			silent_cerr("genetic_algorithm: outputs number must be > 0" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}
		outputs_.reserve(nOut);
		for (int i = 0; i < nOut; ++i) {
			if (!HP.IsKeyWord("node")) {
				silent_cerr("genetic_algorithm: expected 'node' for output #" << (i+1) << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			unsigned olbl = static_cast<unsigned>(HP.GetInt());
			outputs_.push_back(olbl);
		}

		/* Set default GA parameters */
		if (popSize_ < 2)  popSize_ = 2;
		if (numGen_  < 1)  numGen_  = 1;
		if (elitism_ < 0)  elitism_  = 0;
		if (elitism_ >= popSize_) elitism_ = popSize_ - 1;
		if (crossRate_ < 0.) crossRate_ = 0.; if (crossRate_ > 1.) crossRate_ = 1.;
		if (mutRate_   < 0.) mutRate_   = 0.; if (mutRate_   > 1.) mutRate_   = 1.;

		bestGenome_.assign(inputs_.size(), 0.0);

		/* Load libraries & symbols */
		loadLibraries_();

		/* We do NOT run GA in the constructor; we run it once in InitialAssRes()
		 * at t=0 when the simulation starts assembling. This way, the element is
		 * fully constructed before heavy work.
		 */
	}

	virtual ~GeneticAlgorithm() {
		if (hPop_)  dlclose(hPop_);
		if (hCons_) dlclose(hCons_);
	}

private:
	void loadLibraries_() {
		auto open_or_throw = [](const std::string& path)->void* {
			void* h = dlopen(path.c_str(), RTLD_NOW);
			if (!h) {
				silent_cerr("genetic_algorithm: dlopen('" << path << "') failed: " << dlerror() << std::endl);
				throw ErrGeneric(MBDYN_EXCEPT_ARGS);
			}
			return h;
		};

		// Load GA library (contains population generation, crossover, mutation, selection)
		hPop_  = open_or_throw(popLib_);
		
		// Load constraint library
		hCons_ = open_or_throw(consLib_);

		dlerror(); // clear

		// Get function pointers from GA library
		gen_pop_ = reinterpret_cast<gen_pop_fn_t>(dlsym(hPop_, "generate_population"));
		const char* e = dlerror();
		if (e) { 
			silent_cerr("genetic_algorithm: dlsym(generate_population): " << e << std::endl); 
			throw ErrGeneric(MBDYN_EXCEPT_ARGS); 
		}

		// Get fitness function from GA library (since we removed separate fitness lib)
		fitness_ = reinterpret_cast<fitness_fn_t>(dlsym(hPop_, "fitness"));
		e = dlerror();
		if (e || !fitness_) { 
			silent_cerr("genetic_algorithm: dlsym(fitness): " << (e?e:"null") << std::endl); 
			throw ErrGeneric(MBDYN_EXCEPT_ARGS); 
		}

		// Get constraint function
		constraint_ok_ = reinterpret_cast<constraint_fn_t>(dlsym(hCons_, "constraint_ok"));
		e = dlerror();
		if (e || !constraint_ok_) { 
			silent_cerr("genetic_algorithm: dlsym(constraint_ok): " << (e?e:"null") << std::endl); 
			throw ErrGeneric(MBDYN_EXCEPT_ARGS); 
		}
	}

	/* Random helpers (deterministic by seed_) */
	inline unsigned urand_(unsigned& s) {
		/* xorshift32 */
		unsigned x = s ? s : 2463534242u;
		x ^= x << 13; x ^= x >> 17; x ^= x << 5;
		s = x;
		return x;
	}
	inline double drand01_(unsigned& s) {
		return (urand_(s) & 0xFFFFFF) / static_cast<double>(0x1000000u);
	}

	/* Evaluate a single genome with penalty for constraint violation */
	double eval_(const std::vector<double>& g) {
		if (!constraint_ok_(g.data(), static_cast<int>(g.size()))) {
			/* large negative penalty */
			return -1e24 - 1e6 * std::accumulate(g.begin(), g.end(), 0.0);
		}
		return fitness_(g.data(), static_cast<int>(g.size()));
	}

	/* Run GA once, store bestGenome_ and bestFitness_ */
	void runGA_() {
		const int G = static_cast<int>(inputs_.size());
		if (G <= 0) {
			silent_cerr("genetic_algorithm: genome length is zero (no inputs)" << std::endl);
			throw ErrGeneric(MBDYN_EXCEPT_ARGS);
		}

		/* Population as flat array for generator: popSize x G */
		std::vector<double> pop(popSize_ * G, 0.0);
		gen_pop_(pop.data(), popSize_, G);

		std::vector<double> fit(popSize_, -1e300);

		/* Evaluate initial population */
		for (int i = 0; i < popSize_; ++i) {
			std::vector<double> gi(G);
			std::copy(pop.begin() + i*G, pop.begin() + (i+1)*G, gi.begin());
			for (double& v : gi) v = clamp01(v);
			fit[i] = eval_(gi);
		}

		unsigned rng = seed_;

		auto select_parent = [&](const std::vector<double>& f)->int {
			/* roulette selection on shifted positive fitness */
			double fmin = *std::min_element(f.begin(), f.end());
			double shift = (fmin < 0.) ? -fmin + 1e-12 : 0.;
			double sum = 0.;
			for (double v : f) sum += (v + shift);
			if (sum <= 0.) {
				/* fallback: uniform pick */
				return static_cast<int>(urand_(rng) % f.size());
			}
			double r = drand01_(rng) * sum;
			double acc = 0.;
			for (int i = 0; i < (int)f.size(); ++i) {
				acc += (f[i] + shift);
				if (r <= acc) return i;
			}
			return (int)f.size()-1;
		};

		std::vector<double> next(pop.size(), 0.0), nextFit(popSize_, -1e300);

		for (int gen = 0; gen < numGen_; ++gen) {
			/* sort by fitness descending to handle elitism */
			std::vector<int> idx(popSize_);
			for (int i = 0; i < popSize_; ++i) idx[i] = i;
			std::sort(idx.begin(), idx.end(), [&](int a, int b){ return fit[a] > fit[b]; });

			/* keep elites */
			for (int e = 0; e < elitism_; ++e) {
				int i = idx[e];
				std::copy(pop.begin() + i*G, pop.begin() + (i+1)*G, next.begin() + e*G);
				nextFit[e] = fit[i];
			}

			/* fill the rest */
			for (int k = elitism_; k < popSize_; k += 2) {
				int p1 = select_parent(fit);
				int p2 = select_parent(fit);

				/* parents */
				const double* g1 = &pop[p1*G];
				const double* g2 = &pop[p2*G];

				/* children buffers */
				std::vector<double> c1(G), c2(G);

				if (drand01_(rng) < crossRate_) {
					/* single-point crossover */
					int cp = 1 + (int)(drand01_(rng) * (G - 1));
					for (int j = 0; j < G; ++j) {
						if (j < cp) { c1[j] = g1[j]; c2[j] = g2[j]; }
						else        { c1[j] = g2[j]; c2[j] = g1[j]; }
					}
				} else {
					for (int j = 0; j < G; ++j) { c1[j] = g1[j]; c2[j] = g2[j]; }
				}

				/* mutation */
				for (int j = 0; j < G; ++j) {
					if (drand01_(rng) < mutRate_) c1[j] = clamp01(c1[j] + (drand01_(rng) - 0.5)*0.2);
					if (drand01_(rng) < mutRate_) c2[j] = clamp01(c2[j] + (drand01_(rng) - 0.5)*0.2);
				}

				/* write children into next population */
				int slot1 = k;
				int slot2 = std::min(k+1, popSize_-1);

				std::copy(c1.begin(), c1.end(), next.begin() + slot1*G);
				std::copy(c2.begin(), c2.end(), next.begin() + slot2*G);

				/* evaluate */
				nextFit[slot1] = eval_(c1);
				if (slot2 != slot1) nextFit[slot2] = eval_(c2);
			}

			/* swap */
			pop.swap(next);
			fit.swap(nextFit);
		}

		/* find best */
		int ibest = (int)(std::max_element(fit.begin(), fit.end()) - fit.begin());
		bestFitness_ = fit[ibest];
		bestGenome_.assign(inputs_.size(), 0.0);
		std::copy(pop.begin() + ibest*G, pop.begin() + (ibest+1)*G, bestGenome_.begin());
		for (double& v : bestGenome_) v = clamp01(v);

		solved_ = true;
	}

public:
	/* ===== MBDyn virtuals ===== */

	virtual void Output(OutputHandler& OH) const override {
		/* Report best genome & fitness; also tag outputs by the user labels. */
		silent_cout("genetic_algorithm(" << GetLabel() << "): "
		             << (solved_ ? "SOLVED" : "NOT_SOLVED") 
		             << ", best fitness = " << bestFitness_ << std::endl);

		if (solved_) {
			for (size_t i = 0; i < outputs_.size(); ++i) {
				double v = (i < bestGenome_.size()) ? bestGenome_[i] : 0.0;
				silent_cout("  out label " << outputs_[i] << " = " << v << std::endl);
			}
		}
	}

	virtual void WorkSpaceDim(integer* pr, integer* pc) const override {
		*pr = 0; *pc = 0;
	}

	virtual VariableSubMatrixHandler&
	AssJac(VariableSubMatrixHandler& WM, doublereal dCoef,
	       const VectorHandler& X, const VectorHandler& XP) override
	{
		WM.SetNullMatrix();
		return WM;
	}

	virtual SubVectorHandler&
	AssRes(SubVectorHandler& WV, doublereal dCoef,
	       const VectorHandler& X, const VectorHandler& XP) override
	{
		WV.ResizeReset(0);
		return WV;
	}

	virtual unsigned int iGetNumPrivData(void) const override {
		/* expose as many scalars as requested in "output number, ..." */
		return static_cast<unsigned>(outputs_.size());
	}

	/* Many MBDyn UDEs also implement this: */
	virtual doublereal dGetPrivData(unsigned int i) const {
		/* 1-based index per MBDyn convention */
		unsigned k = (i==0) ? 0u : (i-1u);
		if (!solved_ || k >= bestGenome_.size()) return 0.;
		return bestGenome_[k];
	}

	virtual int iGetNumConnectedNodes(void) const override { return 0; }

	virtual void GetConnectedNodes(std::vector<const Node *>& cn) const override {
		cn.resize(0);
	}

	virtual void SetValue(DataManager *pDM, VectorHandler& X, VectorHandler& XP,
	                      SimulationEntity::Hints *ph) override
	{
		/* no runtime settable state */
		NO_OP;
	}

	virtual std::ostream& Restart(std::ostream& out) const override {
		out << "# genetic_algorithm: best_fitness " << bestFitness_ << "\n# best_genome:";
		for (double v : bestGenome_) out << " " << v;
		out << std::endl;
		return out;
	}

	virtual unsigned int iGetInitialNumDof(void) const override { return 0; }

	virtual void InitialWorkSpaceDim(integer* pr, integer* pc) const override {
		*pr = 0; *pc = 0;
	}

	virtual VariableSubMatrixHandler&
	InitialAssJac(VariableSubMatrixHandler& WM, const VectorHandler& X) override
	{
		ASSERT(0); WM.SetNullMatrix(); return WM;
	}

	virtual SubVectorHandler&
	InitialAssRes(SubVectorHandler& WV, const VectorHandler& X) override
	{
		/* Called at t=0 assembly; do the heavy compute once here */
		if (!solved_) {
			try { runGA_(); }
			catch (...) {
				silent_cerr("genetic_algorithm(" << GetLabel() << "): GA failed" << std::endl);
				throw;
			}
		}
		WV.ResizeReset(0);
		return WV;
	}
};

/* ===== module init ===== */
extern "C" int module_init(const char* module_name, void* pdm, void* php)
{
	UserDefinedElemRead* rf = new UDERead<GeneticAlgorithm>;
	if (!SetUDE("genetic_algorithm", rf)) {
		delete rf;
		silent_cerr("genetic_algorithm: module_init('" << (module_name?module_name:"") << "') failed" << std::endl);
		return -1;
	}
	return 0;
}
