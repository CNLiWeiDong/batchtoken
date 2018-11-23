/**
 *
 *  eosio.token
 *  Batch Toekn 批次Token，实现核销追踪源头
 *  liweidong 2018.11.12
 */

#include <libcxx/unordered_map>
#include "eosio.token.hpp"

namespace eosio {

    ACTION token::create( name issuer, uint8_t token_type,
                        asset        maximum_supply )
    {
        require_auth( _self );
        auto sym = maximum_supply.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( maximum_supply.is_valid(), "invalid supply");
        eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");
        eosio_assert( token_type<=1, "token type  does not exist" );

        auto sym_name = sym.code().raw();
        stats statstable( _self, sym_name );
        auto existing = statstable.find( sym_name );
        eosio_assert( existing == statstable.end(), "token with symbol already exists" );

        statstable.emplace( _self, [&]( auto& s ) {
           s.supply.symbol = maximum_supply.symbol;
           s.max_supply    = maximum_supply;
           s.issuer        = issuer;
           s.token_type    = token_type;
        });
    }
    ACTION token::destroy(name user, symbol_code sym, string memo)
    {
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );
        auto sym_name = sym.raw();
        require_auth( user );
        require_recipient( user );

        stats statstable( _self, sym_name );
        const auto& st = statstable.get( sym_name, "token with symbol does not exist, create token before destroy" );

        accounts accounttable( _self, user.value );
        const auto& ut = accounttable.get(sym_name,"User does not have this token");

        eosio_assert( ut.balance.amount >0, "over destroy balance" );
        eosio_assert(ut.balance.amount<=st.supply.amount,"destory amount more than the supply");

        statstable.modify(st,user,[&](auto &s){
            s.supply -= ut.balance;
        });

        save_destroy(user,sym,ut);
        accounttable.erase(ut);
        std::bind(a,_1,_2);
        std::placeholders
        float
    }

    void token::save_destroy(name user, symbol_code sym, account ut)
    {
        auto sym_name = sym.raw();
        destroys destroytable(_self,user.value);
        auto to = destroytable.find( sym_name );
        auto now_time = now();
        for(auto &it : ut.balances)
        {
            it.time_status = now_time;
        }
        if( to == destroytable.end() ) {
            destroytable.emplace( user, [&]( auto& a ){
                a.balance = ut.balance;
                a.balances = ut.balances;
            });
        } else {
            destroytable.modify( to, user, [&]( auto& a ) {
                a.balance += ut.balance;
                for(auto &it : ut.balances)
                {
                    a.balances.push_back(it);

                }
            });
        }
    }
    ACTION token::issue( name to, asset quantity, uint32_t expired_day, string memo )
    {
        auto sym = quantity.symbol;
        eosio_assert( sym.is_valid(), "invalid symbol name" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        auto sym_name = sym.code().raw();
        stats statstable( _self, sym_name );
        auto existing = statstable.find( sym_name );
        eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
        const auto& st = *existing;

        require_auth( st.issuer );
        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must issue positive quantity" );

        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
        eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

        statstable.modify(st,st.issuer,[&](auto &s){
            s.supply += quantity;
        });
        if(st.token_type==0)
            add_balance( st.issuer, quantity, st.issuer );
        else
            batch_first_add_balance(st.issuer,to,quantity,expired_day);

        if( to != st.issuer ) {
           SEND_INLINE_ACTION( *this, transfer, {st.issuer,name("active")}, {st.issuer, to, quantity, memo} );
        }
    }
    void token::transfer( name from,
                          name to,
                          asset        quantity,
                          string       memo )
    {
        INLINE_ACTION_SENDER
        eosio_assert( from != to, "cannot transfer to self" );
        require_auth( from );
        eosio_assert( is_account( to ), "to account does not exist");
        auto sym = quantity.symbol.code().raw();
        stats statstable( _self, sym );
        const auto& st = statstable.get( sym );

        require_recipient( from );
        require_recipient( to );

        eosio_assert( quantity.is_valid(), "invalid quantity" );
        eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
        eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
        eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

        if(st.token_type==0)
        {
            sub_balance( from, quantity );
            add_balance( to, quantity, from );
        }
        else
        {
            batch_sub_add_balance(from,to,quantity);
        }

    }

    void token::sub_balance( name owner, asset value ) {
       accounts from_acnts( _self, owner.value );

       const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
       eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );


       if( from.balance.amount == value.amount ) {
          from_acnts.erase( from );
       } else {
          from_acnts.modify( from, owner, [&]( auto& a ) {
              a.balance -= value;
          });
       }
    }

    void token::add_balance( name owner, asset value, name ram_payer )
    {
       accounts to_acnts( _self, owner.value );
       auto to = to_acnts.find( value.symbol.code().raw() );
       if( to == to_acnts.end() ) {
          to_acnts.emplace( ram_payer, [&]( auto& a ){
            a.balance = value;
          });
       } else {
          to_acnts.modify( to, ram_payer, [&]( auto& a ) {
            a.balance += value;
          });
       }
    }
    void token::batch_sub_add_balance(name owner, name to, asset value)
    {
        accounts from_acnts( _self, owner.value );

        const auto& from = from_acnts.get( value.symbol.code().raw(), "no balance object found" );
        eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );
        eosio_assert( from.balances.size()>0, "no balances list" );

        auto cur_time = now();
        //需要遍历所有的token，因过核销有优先级，过期的token可能排在前面，不能只验证最后一个。
        for(const auto& it : from.balances)
        {
            eosio_assert(it.time_status>cur_time,"have expired token");
        }
        std::vector<token_infos> t_balances;

        if( from.balance.amount == value.amount ) {
            t_balances = from.balances;
            from_acnts.erase( from );
        } else {
            from_acnts.modify( from, owner, [&]( auto& a ) {
                a.balance -= value;
                auto t_value = value;
                for(auto it = a.balances.end()-1;it>=a.balances.begin();it--)
                {
                    if(t_value.amount<=0)
                        break;
                    if(it->balance>t_value)
                    {
                        auto t = *it;
                        t.balance = t_value;
                        t_balances.push_back(t);
                        it->balance -= t_value;
                        break;
                    }
                    if(it->balance==t_value)
                    {
                        t_balances.push_back(*it);
                        a.balances.erase(it);
                        break;
                    }
                    t_value -= it->balance;
                    t_balances.push_back(*it);
                    a.balances.erase(it);
                }
            });
        }
        accounts to_acnts( _self, to.value );
        auto ato = to_acnts.find( value.symbol.code().raw() );
        //排序条件 1 优先级 2 永久token 3 过期时间短的
        auto sort_fun = [](token_infos &a, token_infos &b){
            if(a.priority!=b.priority)
                return a.priority<b.priority;
            else
            {
                if(a.time_status==0||b.time_status==0)
                    return a.time_status==0;
                else
                    return a.time_status>b.time_status;
            }

        };
        if( ato == to_acnts.end() ) {
            to_acnts.emplace( owner, [&]( auto& a ){
                a.balance = value;
                a.balances = t_balances;
                std::sort(a.balances.begin(),a.balances.end(),sort_fun);
            });
        } else {
            to_acnts.modify( ato, owner, [&]( auto& a ) {
                a.balance += value;
                a.balances.insert(a.balances.end(),t_balances.begin(),t_balances.end());
                std::sort(a.balances.begin(),a.balances.end(),sort_fun);
            });
        }
    }
    void token::batch_first_add_balance(name issuer, name merchant, asset value, uint32_t expired_day)
    {
        token_infos tf;
        tf.priority = 0;
        tf.merchant = merchant;
        tf.balance = value;
        tf.time_status = expired_day==0? 0 : now()+expired_day*(24*60*60);

        accounts to_acnts( _self, issuer.value );
        auto to = to_acnts.find( value.symbol.code().raw() );
        if( to == to_acnts.end() ) {
            to_acnts.emplace( issuer, [&]( auto& a ){
                a.balance = value;
                a.balances.push_back(tf);
            });
        } else {
            to_acnts.modify( to, issuer, [&]( auto& a ) {
                a.balance += value;
                a.balances.push_back(tf);
            });
        }

    }
    asset token::get_supply( symbol_code sym )const
    {
        stats statstable( _self, sym.raw() );
        const auto& st = statstable.get( sym.raw() );
        return st.supply;
        cout<
    }

    asset token::get_balance( name owner, symbol_code sym )const
    {
        accounts accountstable( _self, owner.value );
        const auto& ac = accountstable.get( sym.raw() );
        return ac.balance;
    }
} /// namespace eosio
EOSIO_DISPATCH(eosio::token,(create)(issue)(transfer)(destroy))
