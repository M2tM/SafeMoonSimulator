//SafeMoonSimulator by M2tM (Devacor). Absolutely feel free to use and re-distribute this, but keep this message at the top of the project.
//If you produce any video content featuring the output or use of this program, please give me a shout-out!

//Most config values are near the top and you can play with them. You'll want to keep an eye on volume and number of wallets and ensure it all seems
//"realistic" and by that I mean (within 365 days), not exceeding 1-2 billion daily volume, not exceeding a few million wallets, and generally doing a gut-check
//Garbage in == garbage out. This is basically more art than science.

//Additionally, because this is meant to be a throw-away, one-off project, the code is hacky and there are occasional magic variables and so on.
//If you want to play with the macro behaviors, you may want to tinker with the Hype variables as they power the large spikes.
//To modify the micro, take a look at randomHolderFactory which contains some pre-configured behaviors. It should be relatively easy to mix-match or change these.

//If you discover any fun trends or make any improvements definitely share with the community!

//No results or implications from the use of this application represent financial advice. Use your own best judgement when investing.

//--Happy Simulating!

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>
#include <random>
#include <chrono>
#include <functional>
#include <fstream>

//Config Values:
const double MaxSafeMoon = 1'000'000'000'000'000;
const int InitialWalletsPerDay = 7500;
const int TicksPerDay = 24;
const double WalletsGrowthPerDay = 0.027;
const double WalletGrowthRandomness = 0.5;
const size_t HolderSummaryCount = 0;

const double HypeMax = 6.0;
const double HypeDurationMultiple = 1.75;
const size_t HypeCycleDistance = TicksPerDay * 8;
const double HypeCycleDuration = 0.25;
const double HypeMin = .5;

const double HypePriceBonusMax = 4.0;

const double MaxToBuyAtOnce = 250'000;

const double MaxToSellAtOnce = 250'000'000'000;

const size_t TotalDays = 365;

const double PersistentHypeVolumeBonus = 1.1;

const double InitialPersonalHoldings = 66'062'716'446;
const double InitialPersonalBuyIn = 0.00000036;

std::string to_string_custom(double a_num) {
	std::ostringstream streamObj;
	streamObj << std::setprecision(9) << std::fixed;
	streamObj << a_num;
	return streamObj.str();
}


inline float mix(float a_start, float a_end, float a_percent) {
	return (a_percent * (a_end - a_start)) + a_start;
}

inline double mix(double a_start, double a_end, double a_percent, double a_strength) {
	return pow(a_percent, a_strength) * (a_end - a_start) + a_start;
}

inline float mixOut(float a_start, float a_end, float a_percent, float a_strength) {
	return (1.0f - pow(1.0f - a_percent, a_strength)) * (a_end - a_start) + a_start;
}

inline float mixBackAndForth(float a_start, float a_end, float a_percent, float a_strength) {
	if (a_percent < .5f) {
		return mix(a_start, a_end, a_percent * 2.0, a_strength);
	}
	return mixOut(a_end, a_start, (a_percent - .5) * 2.0, a_strength);
}

double randomNumber(double a_min, double a_max) {
	static std::mt19937 gen{ std::random_device{}() };
	return std::uniform_real_distribution<double>(a_min, a_max)(gen);
}

int randomNumber(int a_min, int a_max) {
	static std::mt19937 gen{ std::random_device{}() };
	return std::uniform_int_distribution<int>(a_min, a_max)(gen);
}

class AutomaticMarketMaker {
public:
	AutomaticMarketMaker(double a_sfm, double a_usd) :
		sfm(a_sfm),
		usd(a_usd){
	}

	double sfmPrice() const {
		return usd / sfm;
	}

	double usdPrice() const {
		return sfm / usd;
	}
	
	double sellSFM(double a_sfm) {
		auto fee = a_sfm * .1;
		auto reflectAmount = fee / 2.0;
		auto liquidityPairAmount = reflectAmount / 2.0;

		auto sfmMinusFee = a_sfm - fee;

		sfm += liquidityPairAmount;

		volume += a_sfm / usdPrice();

		auto usdToTransact = sfmMinusFee / usdPrice();

		usd -= usdToTransact;
		sfm += sfmMinusFee;

		cachedReflection += reflectAmount;

		if (usd < 0) {
			throw std::runtime_error("Failed To Sell SFM! Not enough liquidity!");
		}

		return usdToTransact;
	}

	double buySFM(double a_usd) {
		auto fee = a_usd * .1;
		auto reflectAmount = fee / 2.0;
		auto liquidityPairAmount = reflectAmount / 2.0;

		auto usdMinusFee = a_usd - fee;
		usd += liquidityPairAmount;

		cachedReflection += reflectAmount / sfmPrice();

		auto totalSfmToBuy = usdMinusFee / sfmPrice();
		sfm -= totalSfmToBuy;
		usd += usdMinusFee;

		if (sfm < 0) {
			throw std::runtime_error("Failed To Buy SFM! Not enough liquidity!");
		}

		volume += a_usd;

		return totalSfmToBuy;
	}

	double safeMoonTotal() const {
		return sfm;
	}

	double usdTotal() const {
		return usd;
	}

	void addSafeMoon(double a_sfm) {
		sfm += a_sfm;
	}

	//We just want to apply reflection once per "frame" for efficiency reasons, so we cache reflected tokens and allow the
	//simulation to query/clear it with this method and apply it to the wallets.
	double getAndClearReflection() {
		auto result = cachedReflection;
		sfm += result * (sfm / MaxSafeMoon); //reflection currently adds to SFM liquidity.
		cachedReflection = 0.0;
		return result;
	}

	double getAndClearVolume() {
		auto result = volume;
		volume = 0.0;
		return result;
	}

private:
	double cachedReflection = 0.0;

	double sfm;
	double usd;

	double volume = 0.0;
};

class TradingStrategy {
public:
	std::string tag() const { return ""; }
	//return false to stop applying further strategies this frame.
	virtual bool apply(double& sfm, AutomaticMarketMaker& amm) { return true; }

	void setEntry(double a_entryAmount, double a_entryPrice) {
		entryAmount = a_entryAmount;
		entryPrice = a_entryPrice;
	}
protected:
	double entryAmount;
	double entryPrice;
};

class WalletHolder {
public:
	WalletHolder(double a_sfm, double a_entryPrice, std::string a_tag = "Burn Address", std::vector<std::shared_ptr<TradingStrategy>> a_strategies = {}) :
		ourTag(a_tag),
		sfm(a_sfm),
		strategies(a_strategies) {
		for (auto&& strategy : strategies) {
			strategy->setEntry(a_sfm, a_entryPrice);
		}
	}

	virtual std::string tag() const { return ourTag; }

	virtual void update(AutomaticMarketMaker& amm) {
		if (cooldownCheck()) {
			auto firstSfm = sfm;
			for (auto&& strategy : strategies) {
				if (!strategy->apply(sfm, amm)) {
					cooldown = randomNumber(1, TicksPerDay);
					return;
				}
			}
		}
	}

	double balance() const { return sfm; }

	void addBalance(double a_amount) { sfm += a_amount; }

	void addReflection(double a_amount) { sfm += a_amount * (sfm / MaxSafeMoon); }
protected:
	bool cooldownCheck() {
		cooldown = std::max(0, cooldown - 1);
		return cooldown == 0;
	}

	std::string ourTag;
	int cooldown = 0;
	double sfm;
	std::vector<std::shared_ptr<TradingStrategy>> strategies;
};

class StopLoss : public TradingStrategy {
public:
	StopLoss(double a_percentLoss, double a_percentToSell = 1.0f) :
		percentLoss(a_percentLoss),
		percentToSell(a_percentToSell) {
	}

	bool apply(double& sfm, AutomaticMarketMaker& amm) override {
		if (sfm > 0.0 && amm.sfmPrice() <= (entryPrice * percentLoss)) {
			auto amountToSell = std::min(MaxToSellAtOnce, sfm * percentToSell);
			amm.sellSFM(amountToSell);
			sfm -= amountToSell;
			if (amountToSell != MaxToSellAtOnce) {
				entryPrice = amm.sfmPrice(); //reset entry so if we fall percentLoss from here we trigger another sell.
			}
			return false;
		}
		return true;
	}

private:
	double percentLoss;
	double percentToSell;
};

class TrailingStopLoss : public TradingStrategy {
public:
	TrailingStopLoss(double a_percentLoss, double a_percentGainToEngage, double a_percentToSell = 1.0f) :
		percentEngage(a_percentGainToEngage),
		percentLoss(a_percentLoss),
		percentToSell(a_percentToSell) {
	}

	bool apply(double& sfm, AutomaticMarketMaker& amm) override {
		if (engaged) {
			entryPrice = std::max(amm.sfmPrice(), entryPrice);
			if (sfm > 0.0 && amm.sfmPrice() <= (entryPrice * percentLoss)) {
				auto amountToSell = std::min(MaxToSellAtOnce, sfm * percentToSell);
				amm.sellSFM(amountToSell);
				sfm -= amountToSell;
				if (amountToSell != MaxToSellAtOnce) {
					entryPrice = amm.sfmPrice(); //reset entry so if we fall percentLoss from here we trigger another sell.
				}
				return false;
			}
		} else if (amm.sfmPrice() >= (entryPrice * percentEngage)) {
			engaged = true;
		}
		return true;
	}

private:
	bool engaged = false;
	double percentEngage;
	double percentLoss;
	double percentToSell;
};

class TakeProfitOnGain : public TradingStrategy {
public:
	TakeProfitOnGain(double a_percentGain, double a_percentToSell = 1.0f) :
		percentGain(a_percentGain),
		percentToSell(a_percentToSell) {
	}

	bool apply(double& sfm, AutomaticMarketMaker& amm) override {
		if (sfm > 0.0 && amm.sfmPrice() >= (entryPrice * percentGain)) {
			auto amountToSell = std::min(MaxToSellAtOnce, sfm * percentToSell);
			amm.sellSFM(amountToSell);
			sfm -= amountToSell;
			if (amountToSell != MaxToSellAtOnce) {
				entryPrice = amm.sfmPrice(); //reset entry so if we fall percentLoss from here we trigger another sell.
			}
			return false;
		}
		return true;
	}

private:
	double percentGain;
	double percentToSell;
};

class TimedSell : public TradingStrategy {
public:
	TimedSell(std::pair<int, int> a_intervalRange, double a_percentToSell = 1.0f) :
		intervalRange(a_intervalRange),
		percentToSell(a_percentToSell) {
		resetCountdown();
	}

	bool apply(double& sfm, AutomaticMarketMaker& amm) override {
		if (sfm > 0.0 && countDown()) {
			auto amountToSell = std::min(MaxToSellAtOnce, sfm * percentToSell);
			amm.sellSFM(amountToSell);
			sfm -= amountToSell;
			return false;
		}
		return true;
	}

private:
	void resetCountdown() {
		currentCountdown = randomNumber(intervalRange.first, intervalRange.second);
	}
	bool countDown() {
		if (--currentCountdown <= 0) {
			resetCountdown();
			return true;
		}
		return false;
	}

	std::pair<int, int> intervalRange;
	int currentCountdown;
	double percentToSell;
};

class TimedBuy : public TradingStrategy {
public:
	TimedBuy(std::pair<int, int> a_intervalRange, double a_percentOfOriginalHoldingsInUSDToBuy = 1.0f) :
		intervalRange(a_intervalRange),
		percentOfOriginalHoldingsInUSDToBuy(a_percentOfOriginalHoldingsInUSDToBuy) {
		resetCountdown();
	}

	bool apply(double& sfm, AutomaticMarketMaker& amm) override {
		if (countDown()) {
			auto amountToBuyUSD = (entryPrice * entryAmount) * percentOfOriginalHoldingsInUSDToBuy;
			sfm += amm.buySFM(amountToBuyUSD);
			return false;
		}
		return true;
	}

private:
	void resetCountdown() {
		currentCountdown = randomNumber(intervalRange.first, intervalRange.second);
	}
	bool countDown() {
		if (--currentCountdown <= 0) {
			resetCountdown();
			return true;
		}
		return false;
	}

	std::pair<int, int> intervalRange;
	int currentCountdown;
	double percentOfOriginalHoldingsInUSDToBuy;
};

class DipBuy : public TradingStrategy {
public:
	DipBuy(double a_percentLoss, double a_percentOfOriginalHoldingsInUSDToBuy = 1.0f) :
		percentLoss(a_percentLoss),
		percentOfOriginalHoldingsInUSDToBuy(a_percentOfOriginalHoldingsInUSDToBuy) {
	}

	bool apply(double& sfm, AutomaticMarketMaker& amm) override {
		currentTop = std::max(amm.sfmPrice(), currentTop);
		if (cooldownCheck() && (amm.sfmPrice() <= (entryPrice * percentLoss))) {
			auto amountToBuyUSD = (entryPrice * entryAmount) * percentOfOriginalHoldingsInUSDToBuy;
			sfm += amm.buySFM(amountToBuyUSD);
			cooldown = cooldownResetValue;
			currentTop = amm.sfmPrice(); //let's reset the local top now that we've "bought the dip" in addition to the cooldown period of 7 days.
			return false;
		}
		return true;
	}

private:
	bool cooldownCheck() {
		cooldown = std::max(0, cooldown - 1);
		return cooldown == 0;
	}

	bool engaged = false;
	double currentTop = 0.0;
	double percentLoss;
	int cooldown = 0;
	const int cooldownResetValue = 24 * 7;
	double percentOfOriginalHoldingsInUSDToBuy;
};


std::shared_ptr<WalletHolder> randomHolderFactory(double a_sfm, double a_entry) {
	static std::vector<std::function<std::shared_ptr<WalletHolder> (double a_sfm, double a_entry)>> generators {
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Hold Until Exit + StopLoss", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(1.25, 5.0)),
				std::make_shared<StopLoss>(randomNumber(.15, .75))
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Skim Profits + StopLoss", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(1.25, 5.0), .25),
				std::make_shared<StopLoss>(randomNumber(.15, .75))
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "To The Moon Then Out", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(5.00, 30.0))
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "To The Moon + Dip Buy", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(5.00, 30.0)),
				std::make_shared<DipBuy>(randomNumber(.6, .95), randomNumber(.25, 2.0))
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Trailing Gains + StopLoss", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TrailingStopLoss>(randomNumber(.8, .95), randomNumber(2.00, 5.00)),
				std::make_shared<StopLoss>(randomNumber(.15, .75))
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Trailing Gains + StopLoss + DipBuy", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TrailingStopLoss>(randomNumber(.8, .95), randomNumber(2.00, 5.00)),
				std::make_shared<DipBuy>(randomNumber(.6, .95), randomNumber(.25, 2.0)),
				std::make_shared<StopLoss>(randomNumber(.15, .55))
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Trailing Gains + Random Sell", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TrailingStopLoss>(randomNumber(.8, .95), randomNumber(2.00, 5.00), .25),
				std::make_shared<TimedSell>(std::make_pair(24 * 14, 24 * 30), .1)
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Trailing Gains + Random Sell", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TimedBuy>(std::make_pair(24 * 3, 24 * 14), .25),
				std::make_shared<TimedSell>(std::make_pair(24 * 3, 24 * 14), .25)
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Profit Exit + Buying/Selling", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(1.25, 3.5)),
				std::make_shared<StopLoss>(randomNumber(.25, .75)),
				std::make_shared<TimedBuy>(std::make_pair(24 * 3, 24 * 14), .25),
				std::make_shared<TimedSell>(std::make_pair(24 * 3, 24 * 14), .25)
			});
		}},
		{[](double a_sfm, double a_entry) { 
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Profit Taking + Buying/Selling", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(1.25, 3.5), .25),
				std::make_shared<StopLoss>(randomNumber(.25, .75)),
				std::make_shared<TimedBuy>(std::make_pair(24 * 3, 24 * 14), .25),
				std::make_shared<TimedSell>(std::make_pair(24 * 3, 24 * 14), .25)
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Profit Taking + Selling", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TakeProfitOnGain>(randomNumber(1.25, 3.5), .25),
				std::make_shared<TimedSell>(std::make_pair(24 * 3, 24 * 14), .25)
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Trailing Profit + Dip Buy", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<DipBuy>(randomNumber(.8, .95), randomNumber(.25, 2.0)),
				std::make_shared<TrailingStopLoss>(randomNumber(.8, .95), randomNumber(2.00, 3.00), .5)
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Scale Out", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TimedSell>(std::make_pair(24 * 3, 24 * 14), .25)
			});
		}},
		{[](double a_sfm, double a_entry) {
			return std::make_shared<WalletHolder>(a_sfm, a_entry, "Scale Out + Buy Dip", std::vector<std::shared_ptr<TradingStrategy>>{
				std::make_shared<TimedSell>(std::make_pair(24 * 3, 24 * 14), .25),
				std::make_shared<DipBuy>(randomNumber(.8, .95), randomNumber(.25, 2.0))
			});
		}}
	};
	return generators[randomNumber(0, generators.size() - 1)](a_sfm, a_entry);
}


class Simulation {
public:
	Simulation(const AutomaticMarketMaker& a_amm, std::vector<std::shared_ptr<WalletHolder>> a_initialWallets, std::shared_ptr<WalletHolder> a_personalWallet = nullptr) :
		amm(a_amm),
		wallets(a_initialWallets),
		personalWallet(a_personalWallet){

		double amountLeft = MaxSafeMoon - a_amm.safeMoonTotal() - totalWalletAmounts();
		size_t totalHodlers = 350'000 - wallets.size();

		auto firstGeneratedWalletIndex = wallets.size();
		double priorPercent = 0.0;
		for (size_t i = 0; i <= totalHodlers; ++i) {
			auto currentPercent = mix(0.0025, 1.0, static_cast<double>(i) / static_cast<double>(totalHodlers), 1.0);
			auto sfmToAssign = amountLeft * currentPercent;
			amountLeft -= sfmToAssign;
			wallets.push_back(randomHolderFactory(sfmToAssign, a_amm.sfmPrice()));
		}

		//top us off with whatever rounding error we have left
		wallets[firstGeneratedWalletIndex]->addBalance(MaxSafeMoon - a_amm.safeMoonTotal() - totalWalletAmounts());
	}

	void tick() {
		auto walletsToAddThisTick = randomNumber(static_cast<int>(walletsToAdd * WalletGrowthRandomness), walletsToAdd) * getHype();
		for (int i = 0; i < walletsToAddThisTick; ++i) {
			auto bucket = randomNumber(0, 4);
			auto usdToBuySafeMoonWith = randomNumber(0.0, 1.0) > .995 
				? mix(20'000.00, 100'000.00, randomNumber(0.0, 1.0), 1.0) //.5% chance for a "whale" purchase
				: mix(5.0, 5'000.00, randomNumber(0.0, 1.0), 6.0); //Most buys are between 5 and 5,000, weighted on the low end.
			usdToBuySafeMoonWith = std::min(MaxToBuyAtOnce, usdToBuySafeMoonWith * getHypePriceBonus());
			auto sfmToInitializeWith = amm.buySFM(usdToBuySafeMoonWith);
			wallets.push_back(randomHolderFactory(sfmToInitializeWith, amm.sfmPrice()));
		}
		std::cout << "SFM Price: " << amm.sfmPrice() << " SFM LP: " << amm.safeMoonTotal() << " USD LP: " << amm.usdTotal() << "\n";
		walletsToAdd = walletsToAdd + walletsToAdd * (WalletsGrowthPerDay / TicksPerDay);
		for (auto&& wallet : wallets) {
			wallet->update(amm);
		}

		applyReflection();
		++currentTick;
	}

	double getHype() {
		if (currentTick >= currentHypeCycleDistance) {
			auto hypeTick = currentTick - currentHypeCycleDistance;
			auto maxHypeTick = HypeCycleDuration * currentHypeCycleDistance;
			if (hypeTick < maxHypeTick) {
				return mixBackAndForth(1.0, HypeMax, static_cast<double>(hypeTick) / maxHypeTick, 3.0);
			} else {
				currentTick = 0;
				currentHypeCycleDistance *= HypeDurationMultiple;
				walletsToAdd *= PersistentHypeVolumeBonus;
			}
		}
		return mixBackAndForth(1.0, HypeMin, static_cast<double>(currentTick) / static_cast<double>(currentHypeCycleDistance), 1.0);
	}

	double getHypePriceBonus() {
		if (currentTick >= currentHypeCycleDistance) {
			auto hypeTick = currentTick - currentHypeCycleDistance;
			auto maxHypeTick = HypeCycleDuration * currentHypeCycleDistance;
			return mixBackAndForth(1.0, HypePriceBonusMax, static_cast<double>(hypeTick) / maxHypeTick, 3.0);
		}
		return 1.0;
	}

	double currentPrice() const {
		return amm.sfmPrice();
	}

	void summary(int day) {
		std::cout << "____________DAY["<<day<<"]____________\nPrice: " << amm.sfmPrice() << " | Volume: " << inMillions(amm.getAndClearVolume()) << "M | USD Market Cap: " << inBillions(amm.sfmPrice() * (MaxSafeMoon - wallets[0]->balance() - amm.safeMoonTotal())) << "B | Burn: " << inTrillions(burnToday) << "T | Hodlers: " << wallets.size() << "\n";
		burnToday = 0.0;
		if (HolderSummaryCount > 0) {
			std::sort(wallets.begin(), wallets.end(), [](const std::shared_ptr<WalletHolder>& a_lhs, const std::shared_ptr<WalletHolder>& a_rhs) {
				return a_lhs->balance() > a_rhs->balance();
				});
		}

		std::cout << "Top " << HolderSummaryCount << " Holders:\n";
		for (int i = 0; i < std::max(static_cast<size_t>(1), HolderSummaryCount); ++i) {
			std::cout << "(" << i << ") SFM: " << wallets[i]->balance() << "\t|USD: " << (wallets[i]->balance() * amm.sfmPrice()) << "\t|" << wallets[i]->tag() << "\n";
		}

		if (personalWallet) {
			std::cout << "\n...\n--->(Personal) SFM: " << personalWallet->balance() << "\t|USD: " << (personalWallet->balance() * amm.sfmPrice()) << "\n";
		}

		std::cout << "\n______________________________" << std::endl;
	}
private:
	double inTrillions(double input) const {
		return input / 1'000'000'000'000;
	}

	double inBillions(double input) const {
		return input / 1'000'000'000;
	}

	double inMillions(double input) const {
		return input / 1'000'000;
	}

	double totalWalletAmounts() {
		double amount = amm.safeMoonTotal();
		for (auto&& hodler : wallets) {
			amount += hodler->balance();
		}
		return amount;
	}

	void applyReflection() {
		double initialBurnBalance = wallets[0]->balance();
		auto reflectedAmount = amm.getAndClearReflection();
		for (auto&& wallet : wallets) {
			wallet->addReflection(reflectedAmount);
		}
		burnToday += wallets[0]->balance() - initialBurnBalance;
	}

	double burnToday = 0.0;
	int walletsToAdd = InitialWalletsPerDay / TicksPerDay;
	std::vector<std::shared_ptr<WalletHolder>> wallets;
	AutomaticMarketMaker amm;
	std::shared_ptr<WalletHolder> personalWallet;
	size_t currentTick = 0;
	size_t currentHypeCycleDistance = HypeCycleDistance;
};


constexpr double sfmForPriceInUSD(double a_usd, double a_sfmPriceInUSD) {
	return a_usd / a_sfmPriceInUSD;
}

void renderHeader(std::ofstream &display) {
	display << R"(
<!DOCTYPE HTML>
<html>
<head>  
<script>
window.onload = function () {

var chart = new CanvasJS.Chart("chartContainer", {
	animationEnabled: true,
	theme: "light2",
	title:{
		text: "Safe Moon For 365 Days"
	},
	data: [{        
		type: "line",
      	indexLabelFontSize: 16,
		dataPoints: [
)";
}

void renderPricePoint(std::ofstream& display, double price) {
	display << "{ y: " << price << " },\n";
}

void renderFooter(std::ofstream& display, const std::shared_ptr<WalletHolder> &a_personalWallet, double a_finalPrice) {
	display << R"(
		]
	}]
});
chart.render();

}
</script>
</head>
<body>
<div id="chartContainer" style="height: 300px; width: 100%;"></div>
<script src="https://canvasjs.com/assets/script/canvasjs.min.js"></script>
<h2>Personal Gains</h2>)";
	
	auto initialHoldingsUSD = (InitialPersonalHoldings * InitialPersonalBuyIn);
	auto finalHoldingsUSD = (a_personalWallet->balance() * a_finalPrice);
	display << "Initial Holdings: " << InitialPersonalHoldings << " SFM : $" << initialHoldingsUSD << " USD<br/>\n";
	display << "Final Holdings: " << a_personalWallet->balance() << " SFM : $" << finalHoldingsUSD << " USD<br/>\n";
	display << "SFM Reflected: " << (a_personalWallet->balance() - InitialPersonalHoldings) << " | Total USD Gains: " << (finalHoldingsUSD - initialHoldingsUSD) << "<br/>\n";
	display << "Value of Reflected SFM in USD: $" << ((a_personalWallet->balance() - InitialPersonalHoldings) * a_finalPrice) << " USD";

	display << "</body>\n</html>\n";
}

int main() {
	const double startingLiquidityUsd = 22390605.0;
	const double currentSafeMoonPrice = 0.00000064;
	const double minSafeMoonPrice = 0.00000005;

	auto makeInitialHolder = [=](double sfm) {
		return randomHolderFactory(sfm, randomNumber(minSafeMoonPrice, currentSafeMoonPrice));
	};

	auto personalWallet = std::make_shared<WalletHolder>(InitialPersonalHoldings, InitialPersonalBuyIn, "Personal");

	std::vector<std::shared_ptr<WalletHolder>> initialHolders {
		std::make_shared<WalletHolder>(401'252'856'803'308, 0.0), //Burn address
		makeInitialHolder(46'085'289'300'689),
		makeInitialHolder(20'959'130'440'245),
		makeInitialHolder(20'000'000'001'566),
		makeInitialHolder(10'945'903'642'850),
		makeInitialHolder(9'001'988'515'145),
		makeInitialHolder(8'092'786'040'380),
		makeInitialHolder(7'238'372'379'794),
		makeInitialHolder(7'191'679'528'556),
		makeInitialHolder(6'173'385'197'188),
		makeInitialHolder(5'256'797'792'631),
		makeInitialHolder(4'954'790'237'591),
		makeInitialHolder(4'822'635'324'409),
		makeInitialHolder(4'623'459'868'922),
		makeInitialHolder(4'245'274'348'261),
		makeInitialHolder(4'159'218'848'778),
		makeInitialHolder(3'360'946'646'817),
		makeInitialHolder(3'313'887'034'654),
		makeInitialHolder(2'844'660'460'204),
		makeInitialHolder(2'194'755'200'367),
		makeInitialHolder(2'117'410'284'289),
		makeInitialHolder(1'980'620'377'233),
		makeInitialHolder(1'815'490'632'489),
		makeInitialHolder(1'775'068'088'342),
		makeInitialHolder(1'623'910'549'573),
		makeInitialHolder(1'596'446'127'894),
		makeInitialHolder(1'327'620'878'027),
		makeInitialHolder(1'241'386'891'489),
		makeInitialHolder(1'198'915'215'575),
		makeInitialHolder(1'182'216'563'842),
		makeInitialHolder(1'117'529'256'143),
		makeInitialHolder(1'081'671'893'513),
		makeInitialHolder(1'070'470'235'129),
		makeInitialHolder(1'070'032'459'729),
		makeInitialHolder(1'064'944'103'339),
		makeInitialHolder(1'033'861'410'524),
		makeInitialHolder(1'013'292'786'760),
		personalWallet
	};

	std::cout << std::setprecision(9) << std::fixed;

	Simulation simulation(
		AutomaticMarketMaker{ sfmForPriceInUSD(startingLiquidityUsd, 0.00000064), startingLiquidityUsd },
		initialHolders,
		personalWallet
	);

	std::ofstream display("render.html", std::ofstream::trunc);
	display << std::setprecision(9) << std::fixed;
	renderHeader(display);
	for (int d = 0; d < TotalDays; ++d) {
		for (int h = 0; h < TicksPerDay; ++h) {
			simulation.tick();
			renderPricePoint(display, simulation.currentPrice());
		}
		simulation.summary(d);
	}
	renderFooter(display, personalWallet, simulation.currentPrice());
}