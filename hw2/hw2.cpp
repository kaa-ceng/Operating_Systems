#include <iostream>
#include "monitor.h"
#include "hw2.h"

/* You can define any global variable
   function and class definition here.
   Programs will first call initStore() then
   all other functions
*/


// USAGE:

class myMonitor : public Monitor {

   // inherit from Monitor

   //supplier for aaa,bbb,ccc
   Condition customerCV, supplierCV[3];     // condition varibles
   
   struct myStore {
      int capAAA;
      int capBBB;
      int capCCC;
      int maxOrder;
      
      int availAAA;
      int availBBB;
      int availCCC;
   } store;


   //global variables
   bool maySupplyAAA;
   bool maySupplyBBB;
   bool maySupplyCCC;

   int reservedAAA;  //reserved amount of AAA
   int reservedBBB;
   int reservedCCC;
   
   //The total number of items sold via buy() calls must equal the total number of items removed from avail.
   int totalSold = 0;
   int totalRemoved = 0;

   public:
   //constructor
   // pass "this" to cv constructors
   myMonitor() : customerCV(this), supplierCV{Condition(this), Condition(this), Condition(this)} {
      store.capAAA = 0;
      store.capBBB = 0;
      store.capCCC = 0;
      store.maxOrder = 0;
      
      store.availAAA = 0;
      store.availBBB = 0;
      store.availCCC = 0;

      maySupplyAAA = false;
      maySupplyBBB = false;
      maySupplyCCC = false;

      reservedAAA = 0;
      reservedBBB = 0;
      reservedCCC = 0;
   }

   // will initialize the store with the given parameters.
   void initStore(int cA, int cB, int cC, int mO) {
      __synchronized__;

      store.capAAA = cA;
      store.capBBB = cB;
      store.capCCC = cC;
      store.maxOrder = mO;

      // items available
      store.availAAA = cA;
      store.availBBB = cB;
      store.availCCC = cC;

      //std::cout << "initStore done:" << cA << ", " << cB << ", " << cC <<std::endl;
      //std::cout << "max order: " << mO << std::endl;
   }

   // the call by the customer threads.
   void buy(int aA, int aB, int aC) { 
      __synchronized__;

      int maxOrder = store.maxOrder;
      //checking if order exceeds limit
      if (aA > maxOrder || aB > maxOrder || aC > maxOrder) {
         //std::cout << "order exceeds  limit" << std::endl;
         return;
      }

      //order is within limit

      //checking if order exceeds available stock
      //if so, wait for stock to be available
      while (aA > store.availAAA || aB > store.availBBB || aC > store.availCCC) {
         //std::cout << "order exceeds available stock" << std::endl;
         //std::cout << "waiting stock" << std::endl;
         customerCV.wait();
         //std::cout << "stock done, processing order" << std::endl;
      }

      // order within limit and available stock
      //processing order
      
      // removing items from available stock
      store.availAAA -= aA;
      store.availBBB -= aB;
      store.availCCC -= aC;

      // update total sold and removed
      totalSold += aA + aB + aC;
      totalRemoved += aA + aB + aC;

      //std::cout << "order placed: " << aA << " AAA, " << aB << " BBB, " << aC << " CCC" << std::endl;
      //std::cout << "total sold: " << totalSold << std::endl;
      //std::cout << "total removed: " << totalRemoved << std::endl;

      // if total sold items doesnt match the total removed
      if(totalSold != totalRemoved) {
         throw std::runtime_error("sold an removed item count different");
      }
      
      //std::cout << "new stock: " << std::endl;
      //std::cout << "AAA: " << store.availAAA << std::endl;
      //std::cout << "BBB: " << store.availBBB << std::endl;
      //std::cout << "CCC: " << store.availCCC << std::endl;

      //notifying supplier threads stock might be available
      for (int i = 0; i < 3; i++) {
         supplierCV[i].notify();
      }
   }

   // the call by the supplier threads.
   void maysupply(int itype, int n) {
      __synchronized__;

      if (itype == AAA) {

         // if n exceeds available capacity, checking if reserved too, wait for capacity to be available
         while (n > store.capAAA - store.availAAA - reservedAAA) {
            //std::cout << "supply exceeds available capacity AAA." << std::endl;
            //std::cout << "waiting for enough capacity" << std::endl;
            supplierCV[AAA].wait();
            //std::cout << "capacity available" << std::endl;
         }


         // may supply is true for AAA item
         maySupplyAAA = true;

         //reserving n amount of AAA as a promise to supply
         reservedAAA += n;
         //std::cout << "reserved " << n << " amount of AAA" << std::endl;


         //std::cout << "supplier can supply " << n << " amount of AAA" << std::endl;

         //from hw pdf:
         //the capacity for n items is reserved for the supplier
         //Other suppliers of the same item type may block if the remaining capacity is insufficient.
      }


      else if (itype == BBB) {

         while (n > store.capBBB - store.availBBB - reservedBBB) {
            //std::cout << "Error: Supply exceeds available capacity BBB." << std::endl;
            //std::cout << "Waiting for capacity to be available..." << std::endl;
            supplierCV[BBB].wait();
         }

         reservedBBB += n;
         //std::cout << "reserved " << n << " amount of BBB" << std::endl;
         maySupplyBBB = true;

         //std::cout << "supplier can supply " << n << " amount of BBB" << std::endl;
      }


      else if (itype == CCC) {

         while (n > store.capCCC - store.availCCC - reservedCCC) {
            //std::cout << "Error: Supply exceeds available capacity CCC" << std::endl;
            //std::cout << "Waiting for capacity to be available..." << std::endl;
            supplierCV[CCC].wait();
         }

         reservedCCC += n;
         //std::cout << "reserved " << n << " amount of CCC" << std::endl;
         maySupplyCCC = true;

         //std::cout << "supplier can supply " << n << " amount of CCC" << std::endl;
      }
   }

   // the call by the supplier threads.
   void supply(int itype, int n) {
      __synchronized__;

      if (itype == AAA && maySupplyAAA) {
         store.availAAA += n;
         reservedAAA -= n;
         maySupplyAAA = false;

         //std::cout << "supplied " << n << " amount of AAA" << std::endl;
         //std::cout << "new AAA stock: " << store.availAAA << std::endl;
      }

      else if (itype == BBB && maySupplyBBB) {
         store.availBBB += n;
         reservedBBB -= n;
         maySupplyBBB = false;

         //std::cout << "supplied " << n << " amount of BBB" << std::endl;
         //std::cout << "new BBB stock: " << store.availBBB << std::endl;
      }

      else if (itype == CCC && maySupplyCCC) {
         store.availCCC += n;
         reservedCCC -= n;
         maySupplyCCC = false;

         //std::cout << "supplied " << n << " amount of CCC" << std::endl;
         //std::cout << "new CCC stock: " << store.availCCC << std::endl;
      }

      //unblock customer threads if their orders can now done.
      customerCV.notifyAll();
   }

   // puts the current store variables on parameter arrays.
   void monitorStore(int c[3], int a[3]) {
      __synchronized__;

      c[0] = store.capAAA;
      c[1] = store.capBBB;
      c[2] = store.capCCC;

      a[0] = store.availAAA;
      a[1] = store.availBBB;
      a[2] = store.availCCC;
   }
};

myMonitor myMonitorObj;

// will initialize the store with the given parameters.
void initStore(int cA, int cB, int cC, int mO) {
   myMonitorObj.initStore(cA, cB, cC, mO);
}

// the call by the customer threads.
void buy(int aA, int aB, int aC) {
   myMonitorObj.buy(aA, aB, aC);
}

// the call by the supplier threads.
void maysupply(int itype, int n) {
   myMonitorObj.maysupply(itype, n);
}

// the call by the supplier threads.
void supply(int itype, int n) {
   myMonitorObj.supply(itype, n);
}

// puts the current store variables on parameter arrays.
void monitorStore(int c[3], int a[3]) {
   myMonitorObj.monitorStore(c, a);
}
