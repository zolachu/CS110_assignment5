/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <iostream>
#include <algorithm>
#include <thread>
#include <utility>

#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "ostreamlock.h"
#include "string-utils.h"
#include <map>
#include <vector>
using namespace std;


/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what RSS feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = true;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]);
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.
 */
static const size_t kNumFeedWorkers = 8;
static const size_t kNumArticleWorkers = 64;
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
  log(verbose), rssFeedListURI(rssFeedListURI), built(false), feedPool(kNumFeedWorkers), articlePool(kNumArticleWorkers) {}

/**
 * Private Method: processAllFeeds
 * -------------------------------
 * The provided code (commented out, but it compiles) illustrates how one can
 * programmatically drill down through an RSSFeedList to arrive at a collection
 * of RSSFeeds, each of which can be used to fetch the series of articles in that feed.
 *
 * You'll want to erase much of the code below and ultimately replace it with
 * your multithreaded aggregator.
 */



void NewsAggregator::processAllFeeds() {
  map<string, std::unique_ptr<semaphore>> serverPermits;

  ThreadPool poolRSS(kNumFeedWorkers);
  RSSFeedList feedList(rssFeedListURI);
  try {
    feedList.parse();
  } catch (const RSSFeedListException& rfle) {
    log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
  }

  const map<string, string>& feeds = feedList.getFeeds();
  if (feeds.empty()) {
    cout << "Feed list is technically well-formed, but it's empty!" << endl;
    return;
  }

  
  for (auto iter = feeds.begin(); iter != feeds.end(); iter++) {
    poolRSS.schedule([this, iter] {
	string rssUrl = iter->first;
	string rssTitle = iter->second;
	urlsLock.lock();
	if (urlSet.count(rssUrl)) {
	  log.noteSingleFeedDownloadSkipped(rssUrl);
	  urlsLock.unlock();
	} else {
	  urlSet.insert(rssUrl);
	  urlsLock.unlock();
	}
	RSSFeed rssFeed(rssUrl);
	log.noteSingleFeedDownloadBeginning(rssUrl);
	try {
	  rssFeed.parse();
	} catch(const RSSFeedException& e) {
	  log.noteSingleFeedDownloadFailure(rssUrl);
	  return;
	}
	log.noteSingleFeedDownloadEnd(rssUrl);
	const auto& articles = rssFeed.getArticles();
	if (articles.empty()) {
	  cout << "Feed is technically well-formed, but it's empty!" << endl;
	  return;
	}
	ThreadPool poolArticles(kNumArticleWorkers);
        mutex articlesLock;
	map<pair<string, string>, pair<string, vector<string>>> titlesMap;
	for (std::vector<Article>::const_iterator it = articles.begin(); it != articles.end(); it++) {
	  poolArticles.schedule( [this, it, &articlesLock, &titlesMap] {

	      Article article = *it;
	      string articleUrl = article.url;       // .../a.html etc
	      string articleTitle = article.title;
	      string server = getURLServer(articleUrl);  // cs110.stanford.edu ... etc

	      if(seen.find(articleUrl) == seen.end()) {
	       
		seen.insert(articleUrl);

		urlsLock.lock();
		if(urlSet.count(articleUrl)) {
		  log.noteSingleArticleDownloadSkipped(article);
		  urlsLock.unlock();
		} else {
		  urlSet.insert(articleUrl);
		  urlsLock.unlock();
		}

		HTMLDocument document(articleUrl);
		log.noteSingleArticleDownloadBeginning(article);
		try {
		  document.parse();
		} catch(const HTMLDocumentException& e) {
		  log.noteSingleArticleDownloadFailure(article);
		  return;
		}

		const auto& tokens = document.getTokens();
		vector<string> tokensCopy;

		copy(tokens.begin(), tokens.end(), back_inserter(tokensCopy));
		sort(tokensCopy.begin(), tokensCopy.end());

		articlesLock.lock();
		if (titlesMap.count({articleTitle, server})) {   // if the titles map contains article title and the server
		  string existingUrl = titlesMap[{articleTitle, server}].first;
		  auto existingTokens = titlesMap[{articleTitle, server}].second;

		  sort(existingTokens.begin(), existingTokens.end());
		  vector<string> tokenIntersection;
		  set_intersection(tokensCopy.cbegin(), tokensCopy.cend(), existingTokens.cbegin(), existingTokens.cend(), back_inserter(tokenIntersection));

		  string smallestUrl = (existingUrl.compare(articleUrl) >= 0) ? existingUrl : articleUrl;

		
		  titlesMap[{articleTitle, server}] = make_pair(smallestUrl, tokenIntersection);
		  articlesLock.unlock();
		} else { //if title Map doesn't contain, add article url and tokens tuple to the map.

		  titlesMap[make_pair(articleTitle, server)] = make_pair(articleUrl, tokens);
		  articlesLock.unlock();
		}
	      }
	    });
	}
	poolArticles.wait();
	for (auto& element: titlesMap) {
	  indexLock.lock();
	  pair<string, string> title_server = element.first;
	  pair<string, vector<string>> url_tokens = element.second;
	  Article article = {url_tokens.first, title_server.first};
	  index.add(article, url_tokens.second);
	  indexLock.unlock();
	}
	log.noteAllArticlesHaveBeenScheduled(rssUrl);
      });
  }
  poolRSS.wait();
  log.noteAllRSSFeedsDownloadEnd();
}

