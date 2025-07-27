#include <memory>
#include <stdlib.h>
#include <iostream>
#include <string>


#include "mysql_connection.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/connection.h>
#include <cppconn/statement.h>
#include <cppconn/resultset.h>

#include<algorithm>
#include<limits>
#include <Windows.h>
#include<sstream>

#include <cctype>

#include <chrono>



using namespace std;

using namespace sql;
using namespace chrono;


const string server = "tcp://127.0.0.1:3306";
const string username = "WENRER_11"; // Use the WENRER_11 user
const string password = "testpassword"; // Use the password you set for WENRER_11

const string Database = "hydrogen";




class Student {

    string Id;

public:

    Student(): Id(""){}

    void setId(string id)
    {
        Id = id;
    }

    string getId()
    {
        return Id;


    }



};

class Library {

private:

    string Name;

    int Quantity; 




public:

    Library() : Name(""), Quantity(0)
    {

    }

    void setName(string name)

    {
        Name = name;


    }



    void setQuantity(int quantity)
    {
        Quantity = quantity;


    }

    string getName()
    {
        return Name;


    }


    int getQuantity()
    {
        return Quantity;
    }

  string getBookName(sql::Connection* conn, const string& book_name) {
        try {
            unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
                "SELECT NAME FROM lib WHERE UPPER(NAME) = UPPER(?) LIMIT 1"
            ));
            pstmt->setString(1, book_name);
            unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

            if (res->next()) {
              cout << res->getString("NAME")<<"\n"; 
              return  res->getString("NAME");
            }
        }
        catch (sql::SQLException& e) {
            cerr << "SQL Error: " << e.what() << endl;
        }
        return "";
    }

  pair<int, string> getUser(sql::Connection* conn, int user_id, const string& user_name) {
      try {
          unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
              "SELECT S_NO, name FROM student WHERE S_NO = ? AND UPPER(name) = UPPER(?) LIMIT 1"
          ));
          pstmt->setInt(1, user_id);         
          pstmt->setString(2, user_name);   

          unique_ptr<sql::ResultSet> res(pstmt->executeQuery());

          if (res->next()) {
              int stored_id = res->getInt("S_NO"); 
              string stored_name = res->getString("name");
              cout << "User found: ID = " << stored_id << ", Name = " << stored_name << "\n";
              return { stored_id, stored_name };  
          }
      }
      catch (sql::SQLException& e) {
          cerr << "SQL Error: " << e.what() << endl;
      }
      return { -1, "" };  // Return -1 and empty string if not found
  }






  string issueBook(sql::Connection* conn, int student_id, const string& book_name, int day_number, const string& status, double fine_amount) {
      try {
          int i = updateBookQuantity(conn, book_name);

          if (i == -1 || i == 0) {
              cout << "Book is not available.\n";
              return "";
          }

          unique_ptr<sql::PreparedStatement> pstmt(conn->prepareStatement(
              "INSERT INTO issue (student_id, book_name, borrow_date, due_date, status, fine_amount) "
              "VALUES (?, ?, CURDATE(), DATE_ADD(CURDATE(), INTERVAL ? DAY), ?, ?)"
          ));

          pstmt->setInt(1, student_id);
          pstmt->setString(2, book_name);
          pstmt->setInt(3, day_number); // Days to add for due date
          pstmt->setString(4, status);
          pstmt->setDouble(5, fine_amount);

          int rowsAffected = pstmt->executeUpdate();

          if (rowsAffected > 0) {
              // Retrieve the last inserted due_date
              unique_ptr<sql::PreparedStatement> pstmt2(conn->prepareStatement(
                  "SELECT due_date FROM issue WHERE list_number = LAST_INSERT_ID()"
              ));
              unique_ptr<sql::ResultSet> res(pstmt2->executeQuery());

              if (res->next()) {
                  string last_due_date = res->getString("due_date");
                  cout << "Book issued successfully! Due Date: " << last_due_date << "\n";
                  return last_due_date;
              }
          }
      }

    
      catch (sql::SQLException& e) {
          cerr << "SQL Error: " << e.what() << endl;
      }

      return "";  // Return empty string if insertion fails
  }


 int updateBookQuantity(Connection * conn,const string& bookName) {
      try {
        
          // Select the database
          conn->setSchema(Database);

          unique_ptr<PreparedStatement> pstmt(conn->prepareStatement(
              "UPDATE lib SET quantity = quantity - 1 WHERE NAME = ? AND quantity > 0"));

          pstmt->setString(1, bookName);

          int affectedRows = pstmt->executeUpdate();

          if (affectedRows > 0) {
              cout << "Book quantity updated successfully." << endl;
              return 1;
          }
          else {
              cout << "Book not found or out of stock." << endl;
              return 0;
          }
      }
      catch (sql::SQLException& e) {
          cerr << "Error: " << e.what() << endl;
          return -1;
      }
  }





 string returnBook(sql::Connection* conn, int list_number) {
     try {
        
         unique_ptr<sql::PreparedStatement> checkStmt(conn->prepareStatement(
             "SELECT student_id, book_name, due_date, status, fine_amount FROM issue WHERE list_number = ?"
         ));
         checkStmt->setInt(1, list_number);
         unique_ptr<sql::ResultSet> res(checkStmt->executeQuery());

         if (!res->next()) {
             cout << "No record found for the given list number.\n";
             return "No record found";
         }

         string book_name = res->getString("book_name");
         string status = res->getString("status");
         string due_date = res->getString("due_date");
         double fine_amount = res->getDouble("fine_amount");

         if (status == "submit") {
             cout << "Book has already been submitted.\n";
             return "Already submitted";
         }

 
         unique_ptr<sql::PreparedStatement> fineStmt(conn->prepareStatement(
             "SELECT DATEDIFF(CURDATE(), ?) AS overdue_days"
         ));
         fineStmt->setString(1, due_date);
         unique_ptr<sql::ResultSet> fineRes(fineStmt->executeQuery());

         int overdue_days = 0;
         if (fineRes->next()) {
             overdue_days = fineRes->getInt("overdue_days");
         }

         if (overdue_days > 0) {
             fine_amount += overdue_days * 2;  
             cout << "Late return! Fine imposed: rupee " << fine_amount << "\n";
         }
         else {
             fine_amount = 0.0;
             cout << "Book returned on time. No fine imposed.\n";
         }
         unique_ptr<sql::PreparedStatement> updateStmt(conn->prepareStatement(
             "UPDATE issue SET status = 'submit', fine_amount = ? WHERE list_number = ?"
         ));
         updateStmt->setDouble(1, fine_amount);
         updateStmt->setInt(2, list_number);
         updateStmt->executeUpdate();

         unique_ptr<sql::PreparedStatement> updateQtyStmt(conn->prepareStatement(
             "UPDATE lib SET quantity = quantity + 1 WHERE NAME = ?"
         ));
         updateQtyStmt->setString(1, book_name);
         updateQtyStmt->executeUpdate();

         cout << "Book returned successfully!\n";
         return "Book returned. Fine: rupee " + to_string(fine_amount);
     }
     catch (sql::SQLException& e) {
         cerr << "SQL Error: " << e.what() << endl;
         return "Error";
     }
 }




};






void admin (Connection* conn)
 {
      system("cls");

      int val;
      
      cout << "Welcome to Library Management system \n";


      cout << "*\n";
      cout << "1. Add book \n";
      cout << "2.  Add user \n";
      cout << " 3. issue a book\n";
      cout << "4. book return \n";
      cout << "0. Exit\n";
      cout << " Enter your choice:\n";



      cin >> val;


      cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');


      if (val == 1)
      {
          
          system("cls");

              string bookname;

              int quantity;

              cout << "Enter book name :: ";


              getline(cin, bookname);

              cout << " The book name is :: " << bookname;
              cout << "\n";


              cout << "Enter quantity :: ";

              cin >> quantity;

              cout << "\n";


              cin.ignore();


              cout << " The quantity is :: " << quantity;
              cout << "\n";



              sql::PreparedStatement* pstmt = conn->prepareStatement("INSERT INTO lib (name,quantity) Values (?,?)");



              pstmt->setString(1, bookname);

              pstmt->setInt(2, quantity);



              try {
                  pstmt->execute();

                  cout << "Book added successfully!!\n";

          }// try 
              catch (SQLException &e ){

                  cerr << " Error executing statement : " << e.what() << endl;

                  cerr << " MYSQL Error Code : " << e.getErrorCode() << endl;

                  cerr << "SQL State : " << e.getSQLState() << endl;


              }// catch 


              Sleep(3000);


              delete pstmt;

      

      }// if 

      else if (val == 2)
      {
          system("cls");


        int id;
          cout << "Enter Id :: ";
          cin >> id;

          cout << "\n";
          cin.ignore();

          string name;
          cout << "Enter the student name ::  ";
          
          getline(cin, name);
          cout << "\n";

          cout << "Entered  student name  is ::  "<<name <<"\n";


 sql::PreparedStatement* pstmt = conn->prepareStatement("INSERT INTO student ( S_NO,name ) Values (?,?)");/// 28:00 



          pstmt->setInt(1,id);


          pstmt->setString(2, name);





          try {
              pstmt->execute();

              cout << "Student added successfully!!\n";

          }// try 
          catch (SQLException& e) {

              cerr << " Error executing statement : " << e.what() << endl;

              cerr << " MYSQL Error Code : " << e.getErrorCode() << endl;

              cerr << "SQL State : " << e.getSQLState() << endl;


          }// catch 


      


          delete pstmt;
 }

      else if (val == 3)

      {
          cout << " Enter the book name\n";
          string book_name;
          getline(cin, book_name);

          Library hello;

          Driver* driver = get_driver_instance();

          Connection* conn = driver->connect(server, username, password);

          conn->setSchema(Database);





          auto toUpperCase = [](std::string& s) {
              std::transform(s.begin(), s.end(), s.begin(), ::toupper);
              };



          toUpperCase(book_name);

     string name =  hello.getBookName(conn,book_name);

     if (name == book_name)
     {
         cout << " Book is there!!\n";
     }

     else {
         cout << "  books is not there ";
         return;

     }



     cout << "Enter the user name\n";

     string user_name;
     getline(cin, user_name);

     toUpperCase(user_name);
     int student_id;
     cout << "Enter the user ID\n";
     cin >> student_id;
     cin.ignore();

     pair<int, string> user = hello.getUser(conn, student_id, user_name);

     if (user.first != -1) {
         cout << "Student found -> ID: " << user.first << ", Name: " << user.second << "\n";
     }
     else {
         cout << "No student found with ID " << student_id<< " and name '" << user_name << "'.\n";
         return;


     }
     
     
     

    
        string  due_date, status;
        double fine_amount;

        int number_of_days;




        cout << "Enter Status (e.g., 'pending' or 'issued'): ";


        cout << "Enter Fine Amount: ";
        cin >> fine_amount;
        
        cout << "Enter number of days\n";
        cin >> number_of_days;
        cin.ignore();


        string dueDate = hello.issueBook(conn, student_id, book_name, number_of_days, "pending", fine_amount);

     /// 164

  ///  string issueBook(sql::Connection* conn, int student_id, const string& book_name, int day_number, const string& status, double fine_amount) {
      
     
 


       


      }
      else if (val == 4)
      {
          int Id;
          string student_name;
          string book_name;
          int list_number;

          Library hello;

          cout << " Enter the user ID\n";
          cin >> Id;
          cin.ignore();
          cout << "Enter student name\n";
          getline(cin,student_name);
          hello.getUser(conn,Id,student_name);
          cout << "Enter the book_name\n";
          getline(cin, book_name);

          hello.getBookName(conn, book_name);
          cout << "Enter the list_number\n";
          cin >> list_number;
          //  pair<int, string> getUser(sql::Connection* conn, int user_id, const string& user_name)
// string getBookName(sql::Connection* conn, const string& book_name) 

          //599
      
          cout << hello.returnBook(conn,list_number);



      }




      delete  conn;
   return;



}/// admin function ending scope 







void showAllBooks(Connection* con) {
    try {
        if (!con) {
            cerr << "Database connection is null!\n";
            return;
        }

        string query = "SELECT NAME, QUANTITY FROM lib";
        unique_ptr<sql::Statement> stmt(con->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));

        cout << "----- Book List -----\n";
        cout << "NAME\t\tQUANTITY\n";
        cout << "----------------------\n";

        bool hasBooks = false;
        while (res->next()) {
            hasBooks = true;
            cout << res->getString("NAME") << "\t\t" << res->getInt("QUANTITY") << "\n";
        }

        if (!hasBooks) {
            cout << "No books available in the library database.\n";
        }
    }
    catch (sql::SQLException& e) {
        cerr << "SQL Error: " << e.what()
            << " (MySQL Error Code: " << e.getErrorCode()
            << ", SQLState: " << e.getSQLState() << ")\n";
    }
}

//// start  351 






/// 455 

/// new 






int main()
{

    Student s;


    Library l;



    sql::Driver* driver = get_driver_instance();

    
    sql::Connection* conn = driver->connect(server, username, password);

    conn->setSchema(Database);

    cout << "Logged in!" << endl;

    Sleep(3000);





    // Remember to clean up
    // Always delete the connection object when done



    ///
      
    ////







    

      //  ADD USER ''// THEY ARE within admin
        //    ADD BOOK// they are within admin

    ///  borrow book check


   ////////////  --:>
   // 
   admin(conn);// function 1  

    //ADD BOOK DONE 
    // ADD USER  done 


    ///



    








  


/// 1:18:00;

  cout << "\n";
 ///--> working function  showAllBooks( conn);// show all books ;


  
    delete conn;
 
   

    //// entiries in the issue table ;

   



    return 0;

}




//599
