#pragma once

#include "dex_states.hpp"
#include <utils.hpp>


#define ORDER_SN(orderId) orderId&0xFFFFFF

namespace dex {


    inline int64_t power(int64_t base, int64_t exp) {
        int64_t ret = 1;
        while( exp > 0  ) {
            ret *= base; --exp;
        }
        return ret;
    }

    inline int64_t power10(int64_t exp) {
        return power(10, exp);
    }

    inline int64_t calc_precision(int64_t digit) {
        CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
        return power10(digit);
    }

    int64_t calc_asset_amount(const asset &coin_quant, const asset &price, const symbol &asset_symbol) {
        ASSERT(coin_quant.symbol.precision() == price.symbol.precision());
        int128_t precision = calc_precision(asset_symbol.precision());
        return divide_decimal64(coin_quant.amount, price.amount, precision);
    }

    int64_t calc_coin_amount(const asset &asset_quant, const asset &price, const symbol &coin_symbol) {
        ASSERT(coin_symbol.precision() == price.symbol.precision());
        int128_t precision = calc_precision(asset_quant.symbol.precision());
        return multiply_decimal64(asset_quant.amount, price.amount, precision);
    }

    asset calc_asset_quant(const asset &coin_quant, const asset &price, const symbol &asset_symbol) {
        return asset(calc_asset_amount(coin_quant, price, asset_symbol), asset_symbol);
    }

    asset calc_coin_quant(const asset &asset_quant, const asset &price, const symbol &coin_symbol) {
        return asset(calc_coin_amount(asset_quant, price, coin_symbol), coin_symbol);
    }

    inline asset calc_match_fee(int64_t ratio, const asset &quant) {
        if (quant.amount == 0) return asset{0, quant.symbol};
        int64_t fee = multiply_decimal64(quant.amount, ratio, RATIO_PRECISION);
        CHECK(fee < quant.amount, "the calc_fee is large than quantity=" + quant.to_string() + ", ratio=" + to_string(ratio));
        return asset{fee, quant.symbol};
    }

    inline asset calc_match_fee(const dex::order_t &order, const order_side_t &taker_side, const asset &quant) {
        int64_t ratio = (order.order_side == taker_side) ? order.taker_fee_ratio : order.maker_fee_ratio;
        return calc_match_fee(ratio, quant);
    }

    template<typename table_t, typename index_t>
    class table_index_iterator {
    public:
        using table_uptr = std::shared_ptr<table_t>;
        using index_uptr = std::shared_ptr<index_t>;
        using const_iterator = typename index_t::const_iterator;

        table_index_iterator(table_uptr tbl, index_uptr idx, const_iterator itr) :
            tbl(tbl), idx(idx), itr(itr) {}

        table_uptr tbl;
        index_uptr idx;
        const_iterator itr;

        inline bool is_valid() const {
            return itr != idx->end();
        }
    };

    template<typename table_t, typename index_t>
    class matching_order_iterator {
    public:
        using table_index_iterator_ptr = std::shared_ptr<table_index_iterator<table_t, index_t>>;
        matching_order_iterator(table_index_iterator_ptr idx_itr, uint64_t sympair_id, order_side_t side)
            : _idx_itr(idx_itr), _sym_pair_id(sympair_id), _order_side(side)
        {
            process_data();
        };

        void complete_and_next() {
            ASSERT(_idx_itr->is_valid());
            _idx_itr->itr = _idx_itr->idx->erase(_idx_itr->itr);
            process_data();
        }

        void save_matching_order() {        //TODO check matching status
            ASSERT(_idx_itr->is_valid());
            _idx_itr->idx->modify(_idx_itr->itr, same_payer, [&]( auto& a ) {
                a.matched_assets = _matched_assets;
                a.matched_coins = _matched_coins;
                a.matched_fee = _matched_fee;
                a.last_updated_at = current_block_time();
                a.last_deal_id = _last_deal_id;
            });
        }

        inline const order_t &stored_order() {
            return *_idx_itr->itr;
        }

        inline void match(uint64_t deal_id,
                    const asset &new_matched_assets,
                    const asset &new_matched_coins,
                    const asset &new_matched_fee) {

            _last_deal_id   = deal_id;
            _matched_assets += new_matched_assets;
            _matched_coins  += new_matched_coins;
            _matched_fee    += new_matched_fee;
            const auto &order = *_idx_itr->itr;

            CHECK(_matched_assets <= order.limit_quant,
                "The matched assets=" + _matched_assets.to_string() +
                " is overflow with limit_quant=" + order.limit_quant.to_string());
            _complete = _matched_assets == order.limit_quant;

            if (order.order_side == order_side::BUY) {
                auto total_matched_coins = (_matched_coins.symbol == _matched_fee.symbol) ?
                    _matched_coins + _matched_fee // the buyer pay fee with coins
                    : _matched_coins;
                CHECK(total_matched_coins <= order.frozen_quant,
                        "The total_matched_coins=" + _matched_coins.to_string() +
                        " is overflow with frozen_quant=" + order.frozen_quant.to_string() + " for buy order");
                if (_complete) {
                    if (order.frozen_quant > total_matched_coins) {
                        _refund_coins = order.frozen_quant - total_matched_coins;
                    }
                }
            }
        }


        inline asset get_free_limit_quant() const {
            ASSERT(_idx_itr->is_valid());
            asset ret = _idx_itr->itr->limit_quant - _matched_assets;
            ASSERT(ret.amount >= 0);
            return ret;
        }

        inline asset get_refund_coins() const {
            ASSERT(_idx_itr->is_valid());
            return _refund_coins;
        }

        inline const order_side_t &order_side() const {
            return _order_side;
        }

        inline bool is_valid() const {
            return _idx_itr->is_valid();
        }

        inline bool is_completed() const {
            return _complete; 
        }

    private:
        void process_data() {
            if (!_idx_itr->is_valid()) {
                TRACE("matching order itr end! sympair_id=", _sym_pair_id, ", side=", _order_side, "\n");
                return;
            }

            const auto &stored_order = *_idx_itr->itr;
            TRACE("found order! order=", stored_order, "\n");

            _last_deal_id   = stored_order.last_deal_id;
            _matched_assets = stored_order.matched_assets;
            _matched_coins  = stored_order.matched_coins;
            _matched_fee    = stored_order.matched_fee;
            _refund_coins   = asset(0, _matched_coins.symbol);
            _complete       = false;
        }

        table_index_iterator_ptr    _idx_itr;
        uint64_t                    _sym_pair_id;
        order_side_t                _order_side;

        uint64_t                    _last_deal_id = 0;
        asset                       _matched_assets;      //!< total matched asset amount
        asset                       _matched_coins;       //!< total matched coin amount
        asset                       _matched_fee;        //!< total matched fees
        asset                       _refund_coins;
        bool                        _complete;
    };

    template<typename order_iterator_t>
    class matching_pair_iterator {
    public:
        using order_iterator_ptr = std::shared_ptr<order_iterator_t>;

        matching_pair_iterator(const dex::symbol_pair_t& sym_pair, order_iterator_ptr buy_itr, order_iterator_ptr sell_itr )
            : _sym_pair(sym_pair), _buy_itr(buy_itr), _sell_itr(sell_itr)  {
            process_data();
        }

        void complete_and_next() {
            if (_taker_itr->is_completed()) {
                _taker_itr->complete_and_next();
            }
            if (_maker_itr->is_completed()) {
                _maker_itr->complete_and_next();
            }
            process_data();
        }

        void save_matching_order() {
            _buy_itr->save_matching_order( );
            _sell_itr->save_matching_order( );
        }

        bool can_match() const  {
            return _can_match;
        }

        order_iterator_t& maker_it() {
            ASSERT(_can_match);
            return *_maker_itr;
        }
        order_iterator_t& taker_it() {
            ASSERT(_can_match);
            return *_taker_itr;
        }

        void calc_matched_amounts(asset &matched_assets, asset &matched_coins) {
            const auto &asset_symbol    = _sym_pair.asset_symbol.get_symbol();
            const auto &coin_symbol     = _sym_pair.coin_symbol.get_symbol();
            ASSERT( _maker_itr->stored_order().price.amount > 0);

            const auto &matched_price   = _maker_itr->stored_order().price;
            auto maker_free_assets      = _maker_itr->get_free_limit_quant();

            ASSERT(maker_free_assets.symbol == asset_symbol);
            CHECK(maker_free_assets.amount > 0, "MUST: maker_free_assets > 0");

            asset taker_free_assets;

            taker_free_assets = _taker_itr->get_free_limit_quant();
            ASSERT(taker_free_assets.symbol == asset_symbol);
            CHECK(taker_free_assets.amount > 0, "MUST: taker_free_assets > 0");

            matched_assets = (taker_free_assets < maker_free_assets) ? taker_free_assets : maker_free_assets;
            matched_coins = calc_coin_quant(matched_assets, matched_price, coin_symbol);
        }

    private:
        const dex::symbol_pair_t &_sym_pair;

        order_iterator_ptr _buy_itr;
        order_iterator_ptr _sell_itr;
        order_iterator_ptr _taker_itr = nullptr;
        order_iterator_ptr _maker_itr = nullptr;
        bool _can_match = false;

        void process_data() {
            _taker_itr = nullptr;
            _maker_itr = nullptr;
            _can_match = false;

            if (!_can_match) {
                _can_match = true;
                if ( _buy_itr->is_valid() && _sell_itr->is_valid() &&
                     _buy_itr->stored_order().price >= _sell_itr->stored_order().price ) {
                    if ( ORDER_SN(_buy_itr->stored_order().order_id) > ORDER_SN(_sell_itr->stored_order().order_id) ) {
                        _taker_itr = _buy_itr;
                        _maker_itr = _sell_itr;
                    } else {
                        _taker_itr = _sell_itr;
                        _maker_itr = _buy_itr;
                    }
                } else {
                    _can_match = false;
                }
            }

            if (!_can_match) {
                _taker_itr = nullptr;
            }
        }
    };

    template<typename table_t, typename index_t>
    inline auto make_table_index_iterator(std::shared_ptr<table_t> tbl, 
                                            std::shared_ptr<index_t> idx, 
                                            typename index_t::const_iterator itr) {
        return std::make_shared<table_index_iterator<table_t, index_t>>(tbl, idx, itr);
    }

    inline auto make_order_iterator(const name contract,
                                    const dex::symbol_pair_t &sym_pair, 
                                    const order_side_t &side) {

        auto tbl = std::make_shared<order_tbl>(make_order_table( contract, sym_pair.sympair_id, side ));

        auto idx_obj = tbl->get_index<"orderprice"_n>();
        auto idx  = std::make_shared<decltype(idx_obj)>(idx_obj);
        auto itr = make_table_index_iterator(tbl, idx, idx->begin());
        return std::make_shared<matching_order_iterator<order_tbl, decltype(idx_obj)>>(itr, sym_pair.sympair_id, side);
    }

}// namespace dex