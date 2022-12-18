#include <dex.hpp>
#include "dex_const.hpp"
#include "eosio.token.hpp"
// #include "version.hpp"
#include <eosio/transaction.hpp>
#include <eosio/permission.hpp>

static constexpr eosio::name active_permission{"active"_n};

#define ADD_ACTION( items, curr) \
     { dex_contract::deal_action act{ _self, { {_self, active_permission} } };\
	        act.send( items, curr );}

using namespace eosio;
using namespace std;
using namespace dex;


void validate_fee_ratio(int64_t ratio, const string &title) {
    CHECKC(ratio >= 0 && ratio <= FEE_RATIO_MAX, err::PARAM_ERROR,
          "The " + title + " out of range [0, " + std::to_string(FEE_RATIO_MAX) + "]")
}

inline string symbol_to_string(const symbol &s) {
    return std::to_string(s.precision()) + "," + s.code().to_string();
}

inline string symbol_pair_to_string(const symbol &asset_symbol, const symbol &coin_symbol) {
    return symbol_to_string(asset_symbol) + "/" + symbol_to_string(coin_symbol);
}

ACTION dex_contract::init() {

    require_auth( get_self() );

    config conf;
    conf.dex_enabled                = true;
    conf.dex_admin                  = "dexadmin"_n;
    conf.dex_fee_collector          = "dexadmin"_n;
    conf.maker_fee_ratio            = 30;
    conf.taker_fee_ratio            = 30;
    conf.max_match_count            = 50;
    conf.admin_sign_required        = false;

    conf.parent_reward_ratio        = 10;
    conf.grand_reward_ratio         = 5;
    conf.apl_farm_id                 = 0;

    conf.support_quote_symbols.insert(extended_symbol(SYS_TOKEN, SYS_ACCOUNT));
    conf.support_quote_symbols.insert(extended_symbol(MIRROR_USDT, MIRROR_BANK));
    conf.support_quote_symbols.insert(extended_symbol(MIRROR_BNB, MIRROR_BANK));
    conf.support_quote_symbols.insert(extended_symbol(MIRROR_ETH, MIRROR_BANK));
    conf.support_quote_symbols.insert(extended_symbol(MIRROR_BTC, MIRROR_BANK));

    _conf_tbl.set(conf, get_self());
}

void dex_contract::setconfig(const dex::config &conf) {
    require_auth( get_self() );
    CHECKC( is_account(conf.dex_admin),        err::ACCOUNT_INVALID,  "The dex_admin account does not exist");
    CHECKC( is_account(conf.dex_fee_collector),err::ACCOUNT_INVALID,  "The dex_fee_collector account does not exist");
    validate_fee_ratio( conf.maker_fee_ratio,   "maker_fee_ratio");
    validate_fee_ratio( conf.taker_fee_ratio,   "taker_fee_ratio");

    _conf_tbl.set(conf, get_self());
}

void dex_contract::setsympair(const extended_symbol&    asset_symbol,
                              const extended_symbol&    coin_symbol,
                              const asset&              min_asset_quant,
                              const asset&              min_coin_quant,
                              bool                      only_accept_coin_fee,
                              bool                      enabled) {
    require_auth( _config.dex_admin );
    const auto &asset_sym = asset_symbol.get_symbol();
    const auto &coin_sym = coin_symbol.get_symbol();
    auto sympair_tbl = make_sympair_table(get_self());
    CHECKC(is_account(asset_symbol.get_contract()), err::ACCOUNT_INVALID, "The bank account of asset does not exist");
    CHECKC(asset_sym.is_valid(),                    err::SYMBOL_MISMATCH, "Invalid asset symbol");
    CHECKC(is_account(coin_symbol.get_contract()),  err::ACCOUNT_INVALID, "The bank account of coin does not exist");
    CHECKC(coin_sym.is_valid(),                     err::SYMBOL_MISMATCH, "Invalid coin symbol");
    CHECKC(asset_sym.code() != coin_sym.code(),     err::SYMBOL_MISMATCH, "Error: asset_symbol.code() == coin_symbol.code()");
    CHECKC(asset_sym == min_asset_quant.symbol,     err::SYMBOL_MISMATCH, "Incorrect symbol of min_asset_quant");
    CHECKC(coin_sym == min_coin_quant.symbol,       err::SYMBOL_MISMATCH, "Incorrect symbol of min_coin_quant");

    auto index = sympair_tbl.get_index<static_cast<name::raw>(symbols_idx::index_name)>();
    CHECKC( index.find( make_symbols_idx(coin_symbol, asset_symbol) ) == index.end(), err::RECORD_NOT_FOUND, "The reverted symbol pair exist");

    auto it = index.find( make_symbols_idx(asset_symbol, coin_symbol));
    if (it == index.end()) {
        // new sym pair
        auto sympair_id = _global->new_sympair_id();
        CHECKC( sympair_tbl.find(sympair_id) == sympair_tbl.end(), err::RECORD_NOT_FOUND, "The symbol pair id exist");
        sympair_tbl.emplace(get_self(), [&](auto &sym_pair) {
            sym_pair.sympair_id          = sympair_id;
            sym_pair.asset_symbol         = asset_symbol;
            sym_pair.coin_symbol          = coin_symbol;
            sym_pair.min_asset_quant      = min_asset_quant;
            sym_pair.min_coin_quant       = min_coin_quant;
            sym_pair.only_accept_coin_fee = only_accept_coin_fee;
            sym_pair.enabled              = enabled;
        });
    } else {
        CHECKC(it->asset_symbol == asset_symbol,    err::SYMBOL_MISMATCH,  "The asset_symbol mismatch with the existed one");
        CHECKC(it->coin_symbol == coin_symbol,      err::SYMBOL_MISMATCH,  "The asset_symbol mismatch with the existed one");
        sympair_tbl.modify(*it, same_payer, [&](auto &sym_pair) {
            sym_pair.min_asset_quant      = min_asset_quant;
            sym_pair.min_coin_quant       = min_coin_quant;
            sym_pair.only_accept_coin_fee = only_accept_coin_fee;
            sym_pair.enabled              = enabled;
        });
    }
}

void dex_contract::onoffsympair(const uint64_t& sympair_id, const bool& on_off) {
    require_auth( _config.dex_admin );

    auto sympair_tbl = make_sympair_table(_self);
    auto it = sympair_tbl.find(sympair_id);
    CHECKC( it != sympair_tbl.end(),            err::RECORD_NOT_FOUND, "sympair not found: " + to_string(sympair_id) )
    sympair_tbl.modify(*it, same_payer, [&](auto &row) {
        row.enabled                 = on_off;
    });
}

void dex_contract::delsympair(const uint64_t& sympair_id) {
    require_auth( _config.dex_admin );

    auto sympair_tbl = make_sympair_table(_self);
    auto it = sympair_tbl.find(sympair_id);
    CHECKC( it != sympair_tbl.end(),            err::RECORD_NOT_FOUND, "sympair not found: " + to_string(sympair_id) )
    sympair_tbl.erase(it);
}


void dex_contract::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
    CHECK_DEX_ENABLED()
    if (from == get_self()) { return; }
    CHECKC( to == get_self(),                   err::PARAM_ERROR, "Must transfer to this contract")
    CHECKC( quant.amount > 0,                   err::PARAM_ERROR, "The quantity must be positive")

    auto queue_tbl = make_queue_table(get_self());
    auto queue_owner_idx = queue_tbl.get_index<"orderowner"_n>();
    auto order_itr = queue_owner_idx.find(from.value);
    CHECKC( order_itr != queue_owner_idx.end(), err::PARAM_ERROR, "The order not in queue: from=" + from.to_string());

    auto sympair_tbl = make_sympair_table(get_self());
    auto sympair_id = order_itr->sympair_id;
    auto sym_pair_it = sympair_tbl.find(sympair_id);
    CHECKC( sym_pair_it != sympair_tbl.end(),   err::RECORD_NOT_FOUND, "The symbol pair id '" + std::to_string(sympair_id) + "' does not exist")
    CHECKC( sym_pair_it->enabled,               err::STATUS_ERROR, "The symbol pair '" + std::to_string(sympair_id) + " is disabled")

    const auto &asset_symbol = sym_pair_it->asset_symbol.get_symbol();
    const auto &coin_symbol = sym_pair_it->coin_symbol.get_symbol();

    name frozen_bank = (order_itr->order_side == dex::order_side::BUY) ? sym_pair_it->coin_symbol.get_contract() :
            sym_pair_it->asset_symbol.get_contract();

    CHECKC( frozen_bank == get_first_receiver(),    err::PARAM_ERROR, "order asset must transfer from : " + frozen_bank.to_string() )
    CHECKC( order_itr->frozen_quant == quant,       err::STATUS_ERROR, "require quantity is " + order_itr->frozen_quant.to_string() )

    auto order_tbl = make_order_table( get_self(), order_itr->sympair_id, order_itr->order_side );
    TRACE_L ( "order_tbl, scpoe: ", ( order_itr->sympair_id << 8 | uint64_t(order_side::index(order_itr->order_side))));
    auto order_sn = _global->new_order_sn();
    auto order_id = _global->new_order_id(order_sn, order_itr->sympair_id, order_itr->order_side);
    TRACE_L ( "order_tbl, order_id:", order_id);

    // auto _index=     order_tbl.get_index<"orderprice"_n>();

    order_tbl.emplace(_self, [&](auto &order_info) {
        order_info          = *order_itr;
        order_info.order_id = order_id;
        order_info.order_sn = order_sn;
    });
    queue_owner_idx.erase(order_itr);
    
    TRACE_L( "match_sympair begin  ", _config.max_match_count);

    if (_config.max_match_count > 0) {
        uint32_t matched_count = 0;
        TRACE_L( "match_sympair check max_match_count ", _config.max_match_count);
        
        match_sympair(get_self(), *sym_pair_it, _config.max_match_count, matched_count, "oid:" + std::to_string(order_id));
    }
}

void dex_contract::cancel(const uint64_t& pair_id, const name& side, const uint64_t &order_id) {
    CHECK_DEX_ENABLED()
    auto order_tbl = make_order_table(get_self(), pair_id, side);
    auto it = order_tbl.find(order_id);
    CHECKC(it != order_tbl.end(), err::RECORD_NOT_FOUND, "The order does not exist or has been matched");
    auto order = *it;
    // TODO: support the owner auth to cancel order?
    require_auth(order.owner);

    auto sympair_tbl = make_sympair_table(get_self());
    auto sym_pair_it = sympair_tbl.find(order.sympair_id);
    CHECKC( sym_pair_it != sympair_tbl.end(), err::RECORD_NOT_FOUND,
        "The symbol pair id '" + std::to_string(order.sympair_id) + "' does not exist");
    CHECKC( sym_pair_it->enabled,  err::STATUS_ERROR,    "The symbol pair '" + std::to_string(order.sympair_id) + " is disabled")

    asset quantity;
    name bank;
    if (order.order_side == order_side::BUY) {
        quantity = order.frozen_quant - order.matched_coins;
        bank = sym_pair_it->coin_symbol.get_contract();
    } else { // order.order_side == order_side::SELL
        quantity = order.frozen_quant - order.matched_assets;
        bank = sym_pair_it->asset_symbol.get_contract();
    }
    CHECKC(quantity.amount >= 0, err::PARAM_ERROR, "Can not unfreeze the invalid quantity=" + quantity.to_string());
    if (quantity.amount > 0) {
        add_balance(order.owner, bank, quantity, balance_type::ordercancel, "order cancel: " + to_string(order_id));
    }
    order_tbl.erase(it);

}

dex::config dex_contract::get_default_config() {
    return {
        true,                   // bool dex_enabled
        get_self(),             // name admin;
        get_self(),             // name dex_fee_collector;
        DEX_MAKER_FEE_RATIO,    // int64_t maker_fee_ratio;
        DEX_TAKER_FEE_RATIO,    // int64_t taker_fee_ratio;
        DEX_MATCH_COUNT_MAX,    // uint32_t max_match_count
        false,                  // bool admin_sign_required
    };
}

void dex_contract::match(const name &matcher, const uint64_t& sympair_id, uint32_t max_count, const string &memo) {
    CHECK_DEX_ENABLED()

    CHECKC(is_account(matcher),                 err::ACCOUNT_INVALID, "The matcher account does not exist");
    CHECKC(max_count > 0,                       err::PARAM_ERROR, "The max_count must > 0")

    auto sympair_tbl = dex::make_sympair_table(get_self());
   
    auto sym_pair_it = sympair_tbl.find(sympair_id);
    CHECKC(sym_pair_it != sympair_tbl.end(),    err::PARAM_ERROR,  "The symbol pair=" + std::to_string(sympair_id) + " does not exist");
    CHECKC(sym_pair_it->enabled,                err::STATUS_ERROR, "The indicated sym_pair=" + std::to_string(sympair_id) + " is disabled");

    uint32_t matched_count = 0;
    // match_sympair(matcher, *sym_pair_it, max_count, matched_count, memo);
    CHECKC(matched_count > 0,  err::PARAM_ERROR, "None matched");
}

void dex_contract::match_sympair(const name &matcher, const dex::symbol_pair_t &sym_pair,
                                  uint32_t max_count, uint32_t &matched_count, const string &memo) {
    auto cur_block_time     = current_block_time();
    TRACE_L( "match_sympair matching_pair_iterator enter ");
    auto matching_pair_it = dex::matching_pair_iterator( sym_pair,
        dex::make_order_iterator(get_self(), sym_pair, dex::order_side::BUY),
        dex::make_order_iterator(get_self(), sym_pair, dex::order_side::SELL)
    );
    TRACE_L("match_sympair matching_pair_iterator ");
    
    asset latest_deal_price;
    std::list<deal_item_t> items;
    while (matched_count < max_count && matching_pair_it.can_match()) {
        TRACE_L("matched round begin count: " , matched_count);

        auto &maker_it = matching_pair_it.maker_it();
        auto &taker_it = matching_pair_it.taker_it();
        TRACE_L("matching taker_order=", maker_it.stored_order());
        TRACE_L("matching maker_order=", taker_it.stored_order());

        const auto &matched_price = maker_it.stored_order().price;
        latest_deal_price = matched_price;

        asset matched_coins;
        asset matched_assets;
        matching_pair_it.calc_matched_amounts(matched_assets, matched_coins);
        CHECKC(matched_assets.amount > 0 || matched_coins.amount > 0, err::PARAM_ERROR, "Invalid calc_matched_amounts!");
        if (matched_assets.amount == 0 || matched_coins.amount == 0) {
            TRACE_L("Dust calc_matched_amounts! ", PP0(matched_assets), PP(matched_coins));
        }

        auto &buy_it = (taker_it.order_side() == order_side::BUY) ? taker_it : maker_it;
        auto &sell_it = (taker_it.order_side() == order_side::SELL) ? taker_it : maker_it;

        const auto &buy_order   = buy_it.stored_order();
        const auto &sell_order  = sell_it.stored_order();
        asset seller_recv_coins = matched_coins;
        asset buyer_recv_assets = matched_assets;
        const auto &asset_symbol = sym_pair.asset_symbol.get_symbol();
        const auto &coin_symbol = sym_pair.coin_symbol.get_symbol();
        const auto &asset_bank  = sym_pair.asset_symbol.get_contract();
        const auto &coin_bank   = sym_pair.coin_symbol.get_contract();

        asset buy_fee;
        // transfer the buy_fee from buy_order to dex_fee_collector
        if (matched_coins.symbol == buy_order.matched_fee.symbol) {
            buy_fee = calc_match_fee(buy_order, taker_it.order_side(), matched_coins);
            // add_balance(_config.dex_fee_collector, coin_bank, buy_fee, balance_type::orderfee, "fee: " + to_string(buy_order.order_id));
            _allot_fee(buy_order.owner, coin_bank, buy_fee, buy_order.order_id);
        } else {
            buy_fee = calc_match_fee(buy_order, taker_it.order_side(), buyer_recv_assets);
            buyer_recv_assets -= buy_fee;
            // add_balance(_config.dex_fee_collector, asset_bank, buy_fee, balance_type::orderfee, "fee: " + to_string(buy_order.order_id));
            _allot_fee(buy_order.owner, asset_bank, buy_fee, buy_order.order_id);
        }

        auto sell_fee = calc_match_fee(sell_order, taker_it.order_side(), seller_recv_coins);
        seller_recv_coins -= sell_fee;
        // transfer the sell_fee from sell_order to dex_fee_collector
        _allot_fee(sell_order.owner, coin_bank, sell_fee, sell_order.order_id);

        // transfer the coins from buy_order to seller
        add_balance(sell_order.owner, coin_bank, seller_recv_coins,  balance_type::ordermatched,
                " order_id " + to_string(sell_order.order_id) + " deal with " + to_string(buy_order.order_id));

        // transfer the assets from sell_order  to buyer
        add_balance(buy_order.owner, asset_bank, buyer_recv_assets,  balance_type::ordermatched,
                " order_id " + to_string(buy_order.order_id) + " deal with " + to_string(sell_order.order_id));

        auto deal_id = _global->new_deal_item_id();

        buy_it.match(deal_id, matched_assets, matched_coins, buy_fee);
        sell_it.match(deal_id, matched_assets, matched_coins, sell_fee);

        CHECKC(buy_it.is_completed() || sell_it.is_completed(), err::STATUS_ERROR, "Neither buy_order nor sell_order is completed");

        // process refund
        asset buy_refund_coins(0, coin_symbol);


        deal_item_t deal_item;
        deal_item.id            = deal_id;
        deal_item.sympair_id    = sym_pair.sympair_id;        
        deal_item.buy_order_id  = buy_order.order_id;
        deal_item.buy_order_sn  = buy_order.order_sn;
        deal_item.sell_order_id = sell_order.order_id;
        deal_item.sell_order_sn = sell_order.order_sn;
        deal_item.deal_assets   = matched_assets;
        deal_item.deal_coins    = matched_coins;
        deal_item.deal_price    = matched_price;
        deal_item.taker_side    = taker_it.order_side();
        deal_item.buy_fee       = buy_fee;
        deal_item.sell_fee      = sell_fee;
        deal_item.buy_refund_coins = buy_refund_coins;
        deal_item.memo          = memo;
        deal_item.deal_time     = cur_block_time;
        items.push_back(deal_item);
        
        deal_tbl deals(_self, _self.value);
        deals.emplace(_self, [&](auto& row) {
            row = deal_item;
        });

        matched_count++;

        if (buy_it.is_completed()) {
            buy_refund_coins = buy_it.get_refund_coins();
            if (buy_refund_coins.amount > 0) {
                // refund from buy_order to buyer
                add_balance(buy_order.owner, coin_bank, buy_refund_coins,
                    balance_type::orderrefund, " order_id: " + to_string(buy_order.order_id));
            }
        }
        TRACE_L("match round end: " , matched_count);

        matching_pair_it.complete_and_next();

    }
    TRACE_L("match finished: " , matched_count);

    ADD_ACTION( items, time_point_sec(current_time_point()) );

    TRACE_L("save matching order begin");

    matching_pair_it.save_matching_order();
    TRACE_L("save matching order end");

    if (latest_deal_price.amount > 0)
        update_latest_deal_price(sym_pair.sympair_id, latest_deal_price);
}


void dex_contract::adddexdeal(const std::list<dex::deal_item_t>& deal_items, const time_point_sec& curr_ts ) {
    require_auth(get_self());
    require_recipient(get_self());
}

void dex_contract::_allot_fee(const name &from_user, const name& bank, const asset& fee, const uint64_t order_id){
    if(fee.amount <= 0) return;
    auto dex_fee = fee;

    if(_config.parent_reward_ratio >0){
        auto parent = get_account_creator(from_user);
        if(parent != SYS_ACCOUNT) {
            auto parent_reward = fee * _config.parent_reward_ratio / RATIO_PRECISION;
            if(parent_reward.amount > 0){
                dex_fee -= parent_reward;
                add_balance(parent, bank, parent_reward, balance_type::parentreward,
                    "parent reward from: " + from_user.to_string() + " order: " + to_string(order_id));
            }

            if(_config.grand_reward_ratio >0){
                auto grand = get_account_creator(parent);
                auto grand_reward = fee * _config.grand_reward_ratio / RATIO_PRECISION;
                if(grand_reward.amount > 0) {
                    dex_fee -= grand_reward;
                    add_balance(grand, bank, grand_reward, balance_type::grandreward,
                        "grand reward from: " + from_user.to_string() + " order: " + to_string(order_id));
                }
            }
        }
    }

    if(dex_fee.amount > 0){
        add_balance(_config.dex_fee_collector, bank, dex_fee, balance_type::ordermatched, "fee: " + to_string(order_id));
    }

}

void dex_contract::update_latest_deal_price(const uint64_t& sympair_id, const asset& latest_deal_price) {

    TRACE_L("update_latest_deal_price begin");

    auto sympair_tbl = make_sympair_table(_self);
    auto it = sympair_tbl.find( sympair_id );
    CHECKC( it != sympair_tbl.end(), err::PARAM_ERROR, "Err: sympair not found" )

    sympair_tbl.modify(*it, same_payer, [&](auto &row) {
        row.latest_deal_price = latest_deal_price;
    });
    TRACE_L("update_latest_deal_price end");
}

void dex_contract::neworder(const name &user, const uint64_t &sympair_id,
                            const name &order_side, const asset &limit_quant,
                            const asset &frozen_quant, const asset &price,
                            const uint64_t &external_id,
                            const optional<dex::order_config_ex_t> &order_config_ex) {
    // frozen_quant not in use
    new_order(user, sympair_id, order_side, limit_quant, price, external_id, order_config_ex);
}

/**
 * create order to queue
*/
void dex_contract::new_order(const name &user, const uint64_t &sympair_id,
                             const name &order_side, const asset &limit_quant,
                             const optional<asset> &price,
                             const uint64_t &external_id,
                             const optional<dex::order_config_ex_t> &order_config_ex) {
    CHECK_DEX_ENABLED()
    CHECKC(is_account(user), err::ACCOUNT_INVALID, "Account of user=" + user.to_string() + " does not existed");
    require_auth(user);
    if (_config.admin_sign_required || order_config_ex) { require_auth(_config.dex_admin); }

    auto sympair_tbl = make_sympair_table(get_self());
    auto sym_pair_it = sympair_tbl.find(sympair_id);
    CHECKC( sym_pair_it != sympair_tbl.end(),   err::PARAM_ERROR, "The symbol pair id '" + std::to_string(sympair_id) + "' does not exist")
    CHECKC( sym_pair_it->enabled,               err::STATUS_ERROR, "The symbol pair [" + std::to_string(sympair_id) + "] is disabled")

    const auto &asset_symbol    = sym_pair_it->asset_symbol.get_symbol();
    const auto &coin_symbol     = sym_pair_it->coin_symbol.get_symbol();

    auto taker_fee_ratio = _config.taker_fee_ratio;
    auto maker_fee_ratio = _config.maker_fee_ratio;
    if (order_config_ex) {
        taker_fee_ratio = order_config_ex->taker_fee_ratio;
        maker_fee_ratio = order_config_ex->maker_fee_ratio;
        validate_fee_ratio(taker_fee_ratio, "ratio");
        validate_fee_ratio(maker_fee_ratio, "ratio");
    }

    // check price
    if (price) {
        CHECKC(price->symbol == coin_symbol, err::PARAM_ERROR, "The price symbol mismatch with coin_symbol")
        CHECKC( price->amount > 0, err::PARAM_ERROR,  "The price must > 0 for limit order")
    }

    asset frozen_quant;
    if (order_side == dex::order_side::BUY) {
        CHECKC( limit_quant.symbol == asset_symbol, err::PARAM_ERROR,
            "The limit_symbol=" + symbol_to_string(limit_quant.symbol) +
            " mismatch with asset_symbol=" + symbol_to_string(asset_symbol) +
            " for limit buy order");
        ASSERT(price.has_value());
        frozen_quant = dex::calc_coin_quant(limit_quant, *price, coin_symbol);
        if (sym_pair_it->only_accept_coin_fee) {
            frozen_quant += dex::calc_match_fee(taker_fee_ratio, frozen_quant);
        }

    } else { // order_side == order_side::SELL
        CHECKC( limit_quant.symbol == asset_symbol, err::PARAM_ERROR,
                "The limit_symbol=" + symbol_to_string(limit_quant.symbol) +
                    " mismatch with asset_symbol=" + symbol_to_string(asset_symbol) +
                    " for sell order");
        frozen_quant = limit_quant;
    }

    const auto &fee_symbol = (order_side == dex::order_side::BUY && !sym_pair_it->only_accept_coin_fee) ?
            asset_symbol : coin_symbol;

    auto queue_tbl      = make_queue_table(get_self());
    auto acct_idx       = queue_tbl.get_index<"orderowner"_n>();

    CHECKC( acct_idx.find(user.value) == acct_idx.end(), err::PARAM_ERROR, "The user exists: user=" + user.to_string());

    auto cur_block_time = current_block_time();
    auto order_id       = _global->new_queue_order_id();
    queue_tbl.emplace(get_self(), [&](auto &order) {
        order.order_id          = order_id;
        order.external_id       = external_id;
        order.owner             = user;
        order.sympair_id        = sympair_id;
        order.order_side        = order_side;
        order.order_type    = order_type::LIMIT;
        order.price             = price ? *price : asset(0, coin_symbol);
        order.limit_quant       = limit_quant;
        order.frozen_quant      = frozen_quant;
        order.taker_fee_ratio   = taker_fee_ratio;
        order.maker_fee_ratio   = maker_fee_ratio;
        order.matched_assets    = asset(0, asset_symbol);
        order.matched_coins     = asset(0, coin_symbol);
        order.matched_fee       = asset(0, fee_symbol);
        order.created_at        = cur_block_time;
        order.last_updated_at   = cur_block_time;
        order.last_deal_id      = 0;
    });
}

void dex_contract::add_balance(const name &user, const name &bank, const asset &quantity, const name &type, const string& memo){
    switch (type.value)
    {
    case balance_type::ordermatched.value:
    case balance_type::ordercancel.value:
    case balance_type::orderrefund.value:
        TRANSFER(bank, user, quantity, type.to_string() + " : " + memo);
        break;
    case balance_type::orderfee.value:
    case balance_type::parentreward.value:
    case balance_type::grandreward.value:
    {
        auto rewards = make_reward_table(get_self());
        auto it = rewards.find( user.value );

        extended_asset reward_asset = extended_asset(quantity, bank);
        extended_symbol reward_symbol = reward_asset.get_extended_symbol();
        if(it != rewards.end()){
            rewards.modify(*it, _self, [&](auto &row) {
                if(row.rewards.count(reward_symbol) == 0){
                    row.rewards[reward_symbol] = 0;
                }
                row.rewards[reward_symbol] += quantity.amount;
            });
        }
        break;
    }
    default:
        break;
    }
}

void dex_contract::withdraw(const name &user, const name &bank, const asset& quant, const string &memo) {
    CHECK_DEX_ENABLED()
    require_auth(user);
    CHECKC(quant.amount > 0, err::PARAM_ERROR, "quantity must be positive");

    auto reward_tbl = make_reward_table(get_self());
    auto it = reward_tbl.find(user.value);

    CHECKC(it != reward_tbl.end(),err::RECORD_NOT_FOUND, "The user does not exist or has reward")

    extended_symbol ext_symbol = extended_symbol(quant.symbol, bank);

    CHECKC( it->rewards.count(ext_symbol),              err::STATUS_ERROR, "mismatched symbol of reward" )
    CHECKC( it->rewards.at(ext_symbol) >= quant.amount, err::PARAM_ERROR, "overdrawn balance" )
    reward_tbl.modify(*it, _self, [&](auto &row) {
        row.rewards[ext_symbol] -= quant.amount;
        if(row.rewards[ext_symbol] == 0){
            row.rewards.erase(ext_symbol);
        }
    });

    TRANSFER( bank, user, quant, "reward withdraw" )
}

void dex_contract::buy(const name &user, const uint64_t &sympair_id, const asset &quantity,
                            const asset &price, const uint64_t &external_id,
                            const optional<dex::order_config_ex_t> &order_config_ex) {
    new_order(user, sympair_id, order_side::BUY, quantity, price,
              external_id, order_config_ex);
}

void dex_contract::sell(const name &user, const uint64_t &sympair_id, const asset &quantity,
                             const asset &price, const uint64_t &external_id,
                             const optional<dex::order_config_ex_t> &order_config_ex) {
    new_order(user, sympair_id, order_side::SELL, quantity, price,
              external_id, order_config_ex);
}

void dex_contract::cleandata(const uint64_t &max_count) {
    CHECK_DEX_ENABLED()
    auto cur_block_time = current_block_time();
}