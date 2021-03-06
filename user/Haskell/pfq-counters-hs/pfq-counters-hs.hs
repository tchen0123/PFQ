--
--
--  (C) 2011-14 Nicola Bonelli <nicola@pfq.io>
--
--  This program is free software; you can redistribute it and/or modify
--  it under the terms of the GNU General Public License as published by
--  the Free Software Foundation; either version 2 of the License, or
--  (at your option) any later version.
--
--  This program is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
--  GNU General Public License for more details.
--
--  You should have received a copy of the GNU General Public License
--  along with this program; if not, write to the Free Software Foundation,
--  Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
--
--  The full GNU General Public License is included in this distribution in
--  the file called "COPYING".

{-# LANGUAGE DeriveDataTypeable #-}

module Main where

import qualified Network.PFq as Q
-- import Network.PFq.Lang

import Foreign
-- import System.Environment
import System.Time
import System.Exit

import Control.Monad as M
import Control.Applicative
import Control.Concurrent
import Control.Exception

import Data.List.Split
import Data.Data
import Data.Maybe

import qualified Data.Set as S
import qualified Data.Map as M

import System.Console.CmdArgs


data Key = Key Word32 Word32 Word16 Word16
            deriving (Eq, Show)


data State a = State { sCounter :: MVar a,
                       sFlow    :: MVar a,
                       sSet     :: S.Set Key
                     }


-- Command line options
--
data Options = Options
               {
                caplen   :: Int,
                slots    :: Int,
                function :: [String],
                thread   :: [String]
               } deriving (Data, Typeable, Show)


-- default options
--
options = cmdArgsMode $
    Options {
        caplen   = 64,
        slots    = 131072,
        function = [] &= typ "FUNCTION"  &= help "Where FUNCTION = fun[ >-> fun >-> fun][.gid] (ie: steer_ip)",
        thread   = [] &= typ "BINDING" &= help "Where BINDING = gid[.core[.eth0:...:ethx[.queue.queue...]]]"
    } &= summary "PFq multi-threaded packet counter." &= program "pfq-counters"


-- Group Options
--

type Queue = Int
type Gid   = Int

data Binding = Binding {
                         groupId   :: Int,
                         coreNum   :: Int,
                         devs      :: [String],
                         queues    :: [Queue]
                       } deriving (Eq, Show)


makeBinding :: String -> Binding
makeBinding s = case splitOn "." s of
                        []              ->  error "makeBinding: empty string"
                        [g]             ->  Binding (read g) 0 [] [-1]
                        [g, c]          ->  Binding (read g) (read c) [] [-1]
                        [g, c, ds]      ->  Binding (read g) (read c) (splitOn ":" ds) [-1]
                        g : c : ds : qs ->  Binding (read g) (read c) (splitOn ":" ds) (map read qs)


makeFun :: String -> (Gid, [String])
makeFun s =  case splitOn "." s of
                []      -> error "makeFun: empty string"
                [fs]    -> (-1,             map (filter (/= ' ')) $ splitOn ">->" fs)
                fs : n  -> (read $ head n,  map (filter (/= ' ')) $ splitOn ">->" fs)

-- main function
--

main :: IO ()
main = do
    op <- cmdArgsRun options
    putStrLn $ "[pfq] " ++ show op
    cs  <- runThreads op (M.fromList $ map makeFun (function op))
    t   <- getClockTime
    dumpStat cs t


dumpStat :: (RealFrac a) => [MVar a] -> ClockTime -> IO ()
dumpStat cs t0 = do
    threadDelay 1000000
    t <- getClockTime
    cs' <- mapM (`swapMVar` 0) cs
    M.void( when ((-1) `elem` cs') exitFailure)
    let delta = diffUSec t t0
    let rate = (sum cs' * 1000000) / fromIntegral delta
    putStrLn $ "Total rate pkt/sec: " ++ show (truncate rate :: Integer)
    dumpStat cs t


diffUSec :: ClockTime -> ClockTime -> Int
diffUSec t1 t0 = (tdSec delta * 1000000) + truncate ((fromIntegral(tdPicosec delta) / 1000000) :: Double)
                    where delta = diffClockTimes t1 t0


runThreads :: (Num a) => Options -> M.Map Gid [String] -> IO [MVar a]
runThreads op ms =
    forM (thread op) $ \tb -> do
        let binding = makeBinding tb
            sf = M.lookup (groupId binding) ms <|> M.lookup (-1) ms
        c <- newMVar 0
        f <- newMVar 0
        _ <- forkOn (coreNum binding) (
                 handle ((\e -> M.void (putStrLn ("[pfq] Exception: " ++ show e) >> swapMVar c (-1))) :: SomeException -> IO ()) $ do
                 fp <- Q.openNoGroup (caplen op) (slots op)
                 withForeignPtr fp  $ \q -> do
                     Q.joinGroup q (groupId binding) [Q.class_default] Q.policy_shared
                     forM_ (devs binding) $ \dev ->
                       forM_ (queues binding) $ \queue -> do
                         Q.setPromisc q dev True
                         Q.bindGroup q (groupId binding) dev queue
                         when (isJust sf) $ do
                             putStrLn $ "[pfq] Gid " ++ show (groupId binding) ++ " is using computation: " ++ unwords (function op)
                             Q.groupComputationFromString q (groupId binding) (unwords $ function op)
                     Q.enable q
                     M.void (recvLoop q (State c f S.empty))
                 )
        putStrLn $ "[pfq] " ++ show binding ++ " @core " ++ show (coreNum binding) ++ " started!"
        return c


recvLoop :: (Num a) => Ptr Q.PFqTag -> State a -> IO Int
recvLoop q state = do
    netQueue <- Q.read q 20000
    case Q.qLen netQueue of
        0 ->  recvLoop q state
        _ ->  do
              modifyMVar_ (sCounter state) $ \c -> return (c + fromIntegral (Q.qLen netQueue))
              recvLoop q state

