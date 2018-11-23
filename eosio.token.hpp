/**
 *  eosio.token
 *  Batch Toekn 批次Token，实现核销追踪源头
 *  liweidong 2018.11.12
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

//namespace eosiosystem {
//   class system_contract;
//}

namespace eosio {

   using std::string;

   CONTRACT token : public contract {
      public:
         token( name self,name code,datastream<const char*> ds ):contract(self,code,ds){}
         /*
          *  create 创建token，必须由合约部署人调用。
          *  issuer 分发人
          *  token_type token类型
          *  maximum_supply 最大供应量和token属性(token名和小数)
          */
         ACTION create( name issuer, uint8_t token_type,
                      asset        maximum_supply);
         /*
          * issue 销毁用户下的token，并恢复供应量
          * user 销毁用户，自己调用，自己销毁自己的
          * sym   Token名称(不包含小数点信息)
          * memo 备注
          * */
         ACTION issue( name to, asset quantity, uint32_t expired_day, string memo );
         /*
          * transfer 转账
          * from 发起人
          * to 接收人
          * quantity 转移资产，格式为：100.0000 EOS 。包含了Token的数量，小数位数，名称信息
          * memo 备注
          * */
         ACTION transfer( name from,
                        name to,
                        asset        quantity,
                        string       memo );
         /*
          * destroy 记录销毁数据，同时会恢复Toekn的当前供应量
          * user 销毁用户，用户发起销毁自己的Token数据
          * sym  Token名称(不包含小数点信息)
          * memo 备注
          * */
         ACTION destroy(name user, symbol_code sym, string memo);
         /*
          * get_supply 获取供应量，没有封装成Action，目前无作用
          * sym Token名称
          * */
         inline asset get_supply( symbol_code sym )const;
         /*
          * get_balance 获取余额
          * owner 用户
          * sym Token名称
          * */
         inline asset get_balance( name owner, symbol_code sym )const;
        string a;
      private:
         //Token批次信息
         struct token_infos {
            uint8_t priority; //核销优先级
            asset    balance;  //批次数量
            uint32_t time_status; //批次过期时间 0为不过期、销毁后为销毁时间
            name merchant;      //批次分发者
            EOSLIB_SERIALIZE( token_infos, (priority)(balance)(time_status)(merchant) )
         };
         //Token帐户数据
         TABLE account {
            asset    balance;   //Token资产数据
            std::vector<token_infos> balances; //账户下所有批次信息。先按优先级排序，优先级相同时按过期时间排序。
            uint64_t primary_key()const { return balance.symbol.code().raw(); }
         };
         //Token基本数据，总量，名称，小数，当前供应量，类型，分发人
         TABLE currency_stats {
            asset          supply;  //当前供应量
            asset          max_supply; //总量
            name   issuer;  //分发人
            uint8_t token_type;  //0为普通token 1为批次token
            uint64_t primary_key()const { return supply.symbol.code().raw(); }
         };
         //账户当前Token表
         typedef eosio::multi_index<name("accounts"), account> accounts;
         //账户Token销毁表
         typedef eosio::multi_index<name("destroys"), account> destroys;
         //账户Token过期表
         typedef eosio::multi_index<name("outtime"), account> outtimes;
         //Token基本信息数据表
         typedef eosio::multi_index<"stats"_n, currency_stats> stats;
         /*
          * sub_balance 普通Token扣减,转账接口内部调用
          * owner 被扣减账号
          * value 被扣减资产信息
          * */
         void sub_balance( name owner, asset value );
         /*
          * add_balance 普通Token增加，转账接口内部调用
          * owner 增加资产的账号
          * value 增加资产信息
          * ram_payer 内存负担者，即操作者。一般为转账者。
          * */
         void add_balance( name owner, asset value, name ram_payer );
         /*
          * batch_sub_add_balance 批次Token，用户增加与减少批次Token。方法会修改相应账户Token的批次信息，由转账接口内部调用。
          * owner 发起转账者
          * to 接收转账者
          * value 转账资产信息
          * */
         void batch_sub_add_balance(name owner, name to, asset value);
         /*
          * */
         void batch_first_add_balance(name issuer, name merchant, asset value, uint32_t expired_day);
         /*
         * destroy  记录销毁数据
         * user 销毁用户
         * sym  Token名称(不包含小数点信息)
         * ut 用户Token对应的数据库记录，值传递修改零时值，不能修改原有数据库绑定
         * */
         void save_destroy(name user, symbol_code sym, account st);

   };

} /// namespace eosio
