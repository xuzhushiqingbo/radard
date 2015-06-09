//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <BeastConfig.h>
#include <ripple/protocol/Indexes.h>

namespace ripple {

// {
//   ledger_hash : <ledger>
//   ledger_index : <ledger_index>
//   ...
// }
Json::Value doLedgerEntry (RPC::Context& context)
{
    Ledger::pointer lpLedger;
    Json::Value jvResult = RPC::lookupLedger (
        context.params, lpLedger, context.netOps);

    if (!lpLedger)
        return jvResult;

    uint256     uNodeIndex;
    bool        bNodeBinary = false;

    if (context.params.isMember ("index"))
    {
        // XXX Needs to provide proof.
        uNodeIndex.SetHex (context.params["index"].asString ());
        bNodeBinary = true;
    }
    else if (context.params.isMember ("account_root"))
    {
        RippleAddress   naAccount;

        if (!naAccount.setAccountID (
                context.params["account_root"].asString ())
            || !naAccount.getAccountID ())
        {
            jvResult["error"]   = "malformedAddress";
        }
        else
        {
            uNodeIndex
                    = getAccountRootIndex (naAccount.getAccountID ());
        }
    }
    else if (context.params.isMember ("directory"))
    {
        if (!context.params["directory"].isObject ())
        {
            uNodeIndex.SetHex (context.params["directory"].asString ());
        }
        else if (context.params["directory"].isMember ("sub_index")
                 && !context.params["directory"]["sub_index"].isIntegral ())
        {
            jvResult["error"]   = "malformedRequest";
        }
        else
        {
            std::uint64_t  uSubIndex
                    = context.params["directory"].isMember ("sub_index")
                    ? context.params["directory"]["sub_index"].asUInt () : 0;

            if (context.params["directory"].isMember ("dir_root"))
            {
                uint256 uDirRoot;

                uDirRoot.SetHex (context.params["dir_root"].asString ());

                uNodeIndex  = getDirNodeIndex (uDirRoot, uSubIndex);
            }
            else if (context.params["directory"].isMember ("owner"))
            {
                RippleAddress   naOwnerID;

                if (!naOwnerID.setAccountID (
                        context.params["directory"]["owner"].asString ()))
                {
                    jvResult["error"]   = "malformedAddress";
                }
                else
                {
                    uint256 uDirRoot
                            = getOwnerDirIndex (
                                naOwnerID.getAccountID ());
                    uNodeIndex  = getDirNodeIndex (uDirRoot, uSubIndex);
                }
            }
            else
            {
                jvResult["error"]   = "malformedRequest";
            }
        }
    }
    else if (context.params.isMember ("generator"))
    {
        RippleAddress   naGeneratorID;

        if (!context.params["generator"].isObject ())
        {
            uNodeIndex.SetHex (context.params["generator"].asString ());
        }
        else if (!context.params["generator"].isMember ("regular_seed"))
        {
            jvResult["error"]   = "malformedRequest";
        }
        else if (!naGeneratorID.setSeedGeneric (
            context.params["generator"]["regular_seed"].asString ()))
        {
            jvResult["error"]   = "malformedAddress";
        }
        else
        {
            RippleAddress na0Public;      // To find the generator's index.
            RippleAddress naGenerator
                    = RippleAddress::createGeneratorPublic (naGeneratorID);

            na0Public.setAccountPublic (naGenerator, 0);

            uNodeIndex  = getGeneratorIndex (na0Public.getAccountID ());
        }
    }
    else if (context.params.isMember ("offer"))
    {
        RippleAddress   naAccountID;

        if (!context.params["offer"].isObject ())
        {
            uNodeIndex.SetHex (context.params["offer"].asString ());
        }
        else if (!context.params["offer"].isMember ("account")
                 || !context.params["offer"].isMember ("seq")
                 || !context.params["offer"]["seq"].isIntegral ())
        {
            jvResult["error"]   = "malformedRequest";
        }
        else if (!naAccountID.setAccountID (
            context.params["offer"]["account"].asString ()))
        {
            jvResult["error"]   = "malformedAddress";
        }
        else
        {
            uNodeIndex  = getOfferIndex (naAccountID.getAccountID (),
                context.params["offer"]["seq"].asUInt ());
        }
    }
    else if (context.params.isMember ("ripple_state"))
    {
        RippleAddress   naA;
        RippleAddress   naB;
        Currency         uCurrency;
        Json::Value     jvRippleState   = context.params["ripple_state"];

        if (!jvRippleState.isObject ()
            || !jvRippleState.isMember ("currency")
            || !jvRippleState.isMember ("accounts")
            || !jvRippleState["accounts"].isArray ()
            || 2 != jvRippleState["accounts"].size ()
            || !jvRippleState["accounts"][0u].isString ()
            || !jvRippleState["accounts"][1u].isString ()
            || (jvRippleState["accounts"][0u].asString ()
                == jvRippleState["accounts"][1u].asString ())
           )
        {
            jvResult["error"]   = "malformedRequest";
        }
        else if (!naA.setAccountID (
                     jvRippleState["accounts"][0u].asString ())
                 || !naB.setAccountID (
                     jvRippleState["accounts"][1u].asString ()))
        {
            jvResult["error"]   = "malformedAddress";
        }
        else if (!to_currency (
            uCurrency, jvRippleState["currency"].asString ()))
        {
            jvResult["error"]   = "malformedCurrency";
        }
        else
        {
            uNodeIndex  = getRippleStateIndex (
                naA.getAccountID (), naB.getAccountID (), uCurrency);
        }
    }
    else if (context.params.isMember ("dividend"))
    {
        uNodeIndex = getLedgerDividendIndex();
    }
    else if (context.params.isMember ("account_refer"))
    {
        RippleAddress   naAccount;
        
        if (!naAccount.setAccountID (context.params["account_refer"].asString())
            || !naAccount.getAccountID ())
        {
            jvResult["error"]   = "malformedAddress";
        }
        else
        {
            uNodeIndex = getAccountReferIndex(naAccount.getAccountID());
        }
    }
    else if (context.params.isMember("asset")) {
        RippleAddress naAccount;
        Currency uCurrency;
        Json::Value jvAsset = context.params["asset"];

        if (!jvAsset.isObject()
            || !jvAsset.isMember("currency")
            || !jvAsset.isMember("account")
            || !jvAsset["account"].isString()) {
            jvResult["error"] = "malformedRequest";
        } else if (!naAccount.setAccountID(
                       jvAsset["account"].asString())) {
            jvResult["error"] = "malformedAddress";
        } else if (!to_currency(
                       uCurrency, jvAsset["currency"].asString())) {
            jvResult["error"] = "malformedCurrency";
        } else {
            uNodeIndex = getAssetIndex(naAccount.getAccountID(), uCurrency);
        }
    } else if (context.params.isMember("asset_state")) {
        RippleAddress naA;
        RippleAddress naB;
        Currency uCurrency;
        Json::Value jvAssetState = context.params["asset_state"];

        if (!jvAssetState.isObject()
            || !jvAssetState.isMember("currency")
            || !jvAssetState.isMember("accounts")
            || !jvAssetState["accounts"].isArray()
            || 2 != jvAssetState["accounts"].size()
            || !jvAssetState["accounts"][0u].isString()
            || !jvAssetState["accounts"][1u].isString()
            || (jvAssetState["accounts"][0u].asString() == jvAssetState["accounts"][1u].asString())) {
            jvResult["error"] = "malformedRequest";
        } else if (!naA.setAccountID(
                       jvAssetState["accounts"][0u].asString()) ||
                   !naB.setAccountID(
                       jvAssetState["accounts"][1u].asString())) {
            jvResult["error"] = "malformedAddress";
        } else if (!to_currency(
                       uCurrency, jvAssetState["currency"].asString())) {
            jvResult["error"] = "malformedCurrency";
        } else {
            std::uint32_t uDate = jvAssetState.isMember (jss::date) ? jvAssetState[jss::date].asUInt () : 0;
            uNodeIndex = getQualityIndex (
                getAssetStateIndex (naA.getAccountID (), naB.getAccountID (), uCurrency),
                uDate);
        }
    }
    else
    {
        jvResult["error"]   = "unknownOption";
    }

    if (uNodeIndex.isNonZero ())
    {
        auto sleNode = context.netOps.getSLEi (lpLedger, uNodeIndex);

        if (context.params.isMember("binary"))
            bNodeBinary = context.params["binary"].asBool();

        if (!sleNode)
        {
            // Not found.
            // XXX Should also provide proof.
            jvResult["error"]       = "entryNotFound";
        }
        else if (bNodeBinary)
        {
            // XXX Should also provide proof.
            Serializer s;

            sleNode->add (s);

            jvResult["node_binary"] = strHex (s.peekData ());
            jvResult["index"]       = to_string (uNodeIndex);
        }
        else
        {
            jvResult["node"]        = sleNode->getJson (0);
            jvResult["index"]       = to_string (uNodeIndex);
        }
    }

    return jvResult;
}

} // ripple
