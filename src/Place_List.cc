/*
  This file is part of the FRED system.

  Copyright (c) 2010-2015, University of Pittsburgh, John Grefenstette,
  Shawn Brown, Roni Rosenfield, Alona Fyshe, David Galloway, Nathan
  Stone, Jay DePasse, Anuroop Sriram, and Donald Burke.

  Licensed under the BSD 3-Clause license.  See the file "LICENSE" for
  more information.
*/

//
//
// File: Place_List.cc
//
#include <algorithm>
#include <iostream>
#include <limits>
#include <math.h>
#include <new>
#include <set>
#include <string>
#include <typeinfo>
#include <unistd.h>

#include "Census_Tract.h"
#include "Classroom.h"
#include "Condition.h"
#include "County.h"
#include "Date.h"
#include "Geo.h"
#include "Global.h"
#include "Hospital.h"
#include "Household.h"
#include "Neighborhood.h"
#include "Neighborhood_Layer.h"
#include "Neighborhood_Patch.h"
#include "Office.h"
#include "Params.h"
#include "Person.h"
#include "Place.h"
#include "Place_List.h"
#include "Population.h"
#include "Regional_Layer.h"
#include "Regional_Patch.h"
#include "Random.h"
#include "School.h"
#include "Seasonality.h"
#include "Tracker.h"
#include "Travel.h"
#include "Utils.h"
#include "Visualization_Layer.h"
#include "Workplace.h"

// Place_List::quality_control implementation is very large,
// include from separate .cc file:
#include "Place_List_Quality_Control.cc"

using namespace std;

typedef std::map<int, int> HospitalIDCountMapT;

bool Place_List::Static_variables_set = false;

// mean size of "household" associated with group quarters
double Place_List::College_dorm_mean_size = 3.5;
double Place_List::Military_barracks_mean_size = 12;
double Place_List::Prison_cell_mean_size = 1.5;
double Place_List::Nursing_home_room_mean_size = 1.5;

// non-resident staff for group quarters
int Place_List::College_fixed_staff = 0;
double Place_List::College_resident_to_staff_ratio = 0;
int Place_List::Prison_fixed_staff = 0;
double Place_List::Prison_resident_to_staff_ratio = 0;
int Place_List::Nursing_home_fixed_staff = 0;
double Place_List::Nursing_home_resident_to_staff_ratio = 0;
int Place_List::Military_fixed_staff = 0;
double Place_List::Military_resident_to_staff_ratio = 0;
int Place_List::School_fixed_staff = 0;
double Place_List::School_student_teacher_ratio = 0;

int Place_List::Shelter_duration_mean = 0;
int Place_List::Shelter_duration_std = 0;
int Place_List::Shelter_delay_mean = 0;
int Place_List::Shelter_delay_std = 0;
double Place_List::Pct_households_sheltering = 0;
bool Place_List::High_income_households_sheltering = 0;
double Place_List::Early_shelter_rate = 0.0;
double Place_List::Shelter_decay_rate = 0.0;
bool Place_List::Household_hospital_map_file_exists = false;
int Place_List::Hospital_fixed_staff = 1.0;
double Place_List::Hospital_worker_to_bed_ratio = 1.0;
double Place_List::Hospital_outpatients_per_day_per_employee = 0.0;
double Place_List::Healthcare_clinic_outpatients_per_day_per_employee = 0;
int Place_List::Hospital_min_bed_threshold = 0;
double Place_List::Hospitalization_radius = 0.0;
int Place_List::Hospital_overall_panel_size = 0;
int Place_List::Enable_copy_files = 0;

//HAZEL Parameters needed for multiple place types (not just hospitals)
int Place_List::HAZEL_disaster_start_sim_day = -1;
int Place_List::HAZEL_disaster_end_sim_day = -1;
int Place_List::HAZEL_disaster_evac_start_offset = 0;
int Place_List::HAZEL_disaster_evac_end_offset = 0;
int Place_List::HAZEL_disaster_return_start_offset = 0;
int Place_List::HAZEL_disaster_return_end_offset = 0;
double Place_List::HAZEL_disaster_evac_prob_per_day = 0.0;
double Place_List::HAZEL_disaster_return_prob_per_day = 0.0;
int Place_List::HAZEL_mobile_van_max = 0;

HospitalIDCountMapT Place_List::Hospital_ID_total_assigned_size_map;
HospitalIDCountMapT Place_List::Hospital_ID_current_assigned_size_map;

double distance_between_places(Place* p1, Place* p2) {
  return Geo::xy_distance(p1->get_latitude(), p1->get_longitude(), p2->get_latitude(), p2->get_longitude());
}

Place_List::~Place_List() {
  delete_place_label_map();
}

void Place_List::init_place_type_name_lookup_map() {
  this->place_type_name_lookup_map[Place::TYPE_NEIGHBORHOOD] = "NEIGHBORHOOD";
  this->place_type_name_lookup_map[Place::TYPE_HOUSEHOLD] = "HOUSEHOLD";
  this->place_type_name_lookup_map[Place::TYPE_SCHOOL] = "SCHOOL";
  this->place_type_name_lookup_map[Place::TYPE_CLASSROOM] = "CLASSROOM";
  this->place_type_name_lookup_map[Place::TYPE_WORKPLACE] = "WORKPLACE";
  this->place_type_name_lookup_map[Place::TYPE_OFFICE] = "OFFICE";
  this->place_type_name_lookup_map[Place::TYPE_HOSPITAL] = "HOSPITAL";
  this->place_type_name_lookup_map[Place::TYPE_COMMUNITY] = "COMMUNITY";
}

void Place_List::get_parameters() {

  if(!Place_List::Static_variables_set) {

    // get static parameters for all place subclasses
    Household::get_parameters();
    Neighborhood::get_parameters();
    School::get_parameters();
    Classroom::get_parameters();
    Workplace::get_parameters();
    Office::get_parameters();
    Hospital::get_parameters();

    Params::get_param_from_string("enable_copy_files", &Place_List::Enable_copy_files);

    // geography
    Params::get_param_from_string("msa_file", Place_List::MSA_file);
    Params::get_param_from_string("counties_file", Place_List::Counties_file);
    Params::get_param_from_string("states_file", Place_List::States_file);

    // population parameters
    Params::get_param_from_string("synthetic_population_directory", Global::Synthetic_population_directory);
    Params::get_param_from_string("synthetic_population_id", Global::Synthetic_population_id);
    Params::get_param_from_string("synthetic_population_version", Global::Synthetic_population_version);
    Params::get_param_from_string("city", Global::City);
    Params::get_param_from_string("county", Global::County);
    Params::get_param_from_string("state", Global::US_state);
    Params::get_param_from_string("fips", Global::FIPS_code);
    Params::get_param_from_string("msa", Global::MSA_code);

    // school staff size
    Params::get_param_from_string("school_fixed_staff", &Place_List::School_fixed_staff);
    Params::get_param_from_string("school_student_teacher_ratio", &Place_List::School_student_teacher_ratio);

    if(Global::Enable_Group_Quarters) {
      // group quarter parameters
      Params::get_param_from_string("college_dorm_mean_size", &Place_List::College_dorm_mean_size);
      Params::get_param_from_string("military_barracks_mean_size", &Place_List::Military_barracks_mean_size);
      Params::get_param_from_string("prison_cell_mean_size", &Place_List::Prison_cell_mean_size);
      Params::get_param_from_string("nursing_home_room_mean_size", &Place_List::Nursing_home_room_mean_size);

      Params::get_param_from_string("college_fixed_staff", &Place_List::College_fixed_staff);
      Params::get_param_from_string("college_resident_to_staff_ratio", &Place_List::College_resident_to_staff_ratio);
      Params::get_param_from_string("prison_fixed_staff", &Place_List::Prison_fixed_staff);
      Params::get_param_from_string("prison_resident_to_staff_ratio", &Place_List::Prison_resident_to_staff_ratio);
      Params::get_param_from_string("nursing_home_fixed_staff", &Place_List::Nursing_home_fixed_staff);
      Params::get_param_from_string("nursing_home_resident_to_staff_ratio", &Place_List::Nursing_home_resident_to_staff_ratio);
      Params::get_param_from_string("military_fixed_staff", &Place_List::Military_fixed_staff);
      Params::get_param_from_string("military_resident_to_staff_ratio", &Place_List::Military_resident_to_staff_ratio);
    }

    // household shelter parameters
    if(Global::Enable_Household_Shelter) {
      Params::get_param_from_string("shelter_in_place_duration_mean", &Place_List::Shelter_duration_mean);
      Params::get_param_from_string("shelter_in_place_duration_std", &Place_List::Shelter_duration_std);
      Params::get_param_from_string("shelter_in_place_delay_mean", &Place_List::Shelter_delay_mean);
      Params::get_param_from_string("shelter_in_place_delay_std", &Place_List::Shelter_delay_std);
      Params::get_param_from_string("shelter_in_place_compliance", &Place_List::Pct_households_sheltering);
      int temp_int;
      Params::get_param_from_string("shelter_in_place_by_income", &temp_int);
      Place_List::High_income_households_sheltering = (temp_int == 0 ? false : true);
      Params::get_param_from_string("shelter_in_place_early_rate", &Place_List::Early_shelter_rate);
      Params::get_param_from_string("shelter_in_place_decay_rate", &Place_List::Shelter_decay_rate);
    }

    // household evacuation parameters
    if(Global::Enable_HAZEL) {
      Params::get_param_from_string("HAZEL_disaster_start_sim_day", &Place_List::HAZEL_disaster_start_sim_day);
      Params::get_param_from_string("HAZEL_disaster_end_sim_day", &Place_List::HAZEL_disaster_end_sim_day);
      Params::get_param_from_string("HAZEL_disaster_evac_start_offset", &Place_List::HAZEL_disaster_evac_start_offset);
      Params::get_param_from_string("HAZEL_disaster_evac_end_offset", &Place_List::HAZEL_disaster_evac_end_offset);
      Params::get_param_from_string("HAZEL_disaster_return_start_offset", &Place_List::HAZEL_disaster_return_start_offset);
      Params::get_param_from_string("HAZEL_disaster_return_end_offset", &Place_List::HAZEL_disaster_return_end_offset);
      Params::get_param_from_string("HAZEL_disaster_evac_prob_per_day", &Place_List::HAZEL_disaster_evac_prob_per_day);
      Params::get_param_from_string("HAZEL_disaster_return_prob_per_day", &Place_List::HAZEL_disaster_return_prob_per_day);
      Params::get_param_from_string("HAZEL_mobile_van_max", &Place_List::HAZEL_mobile_van_max);
    }
    if(Global::Enable_Hospitals) {
      Params::get_param_from_string("hospital_worker_to_bed_ratio", &Place_List::Hospital_worker_to_bed_ratio);
      Place_List::Hospital_worker_to_bed_ratio = (Place_List::Hospital_worker_to_bed_ratio == 0.0 ? 1.0 : Place_List::Hospital_worker_to_bed_ratio);
      Params::get_param_from_string("hospital_outpatients_per_day_per_employee", &Place_List::Hospital_outpatients_per_day_per_employee);
      Params::get_param_from_string("healthcare_clinic_outpatients_per_day_per_employee", &Place_List::Healthcare_clinic_outpatients_per_day_per_employee);
      Params::get_param_from_string("hospital_min_bed_threshold", &Place_List::Hospital_min_bed_threshold);
      Params::get_param_from_string("hospitalization_radius", &Place_List::Hospitalization_radius);
      Params::get_param_from_string("hospital_fixed_staff", &Place_List::Hospital_fixed_staff);
    }
  }
  Place_List::Static_variables_set = true;

  if(Global::Enable_Hospitals) {
    char hosp_file_dir[FRED_STRING_SIZE];
    char hh_hosp_map_file_name[FRED_STRING_SIZE];

    Params::get_param_from_string("household_hospital_map_file_directory", hosp_file_dir);
    Params::get_param_from_string("household_hospital_map_file", hh_hosp_map_file_name);

    if(strcmp(hh_hosp_map_file_name, "none") == 0) {
      Place_List::Household_hospital_map_file_exists = false;
    } else {
      //If there is a file mapping Households to Hospitals, open it
      FILE* hospital_household_map_fp = NULL;

      char filename[FRED_STRING_SIZE];

      sprintf(filename, "%s%s", hosp_file_dir, hh_hosp_map_file_name);

      hospital_household_map_fp = Utils::fred_open_file(filename);
      if(hospital_household_map_fp != NULL) {
        Place_List::Household_hospital_map_file_exists = true;
        enum column_index {
          hh_label = 0, hospital_label = 1
        };
        char line_str[255];
        Utils::Tokens tokens;
        for(char* line = line_str; fgets(line, 255, hospital_household_map_fp); line = line_str) {
          tokens = Utils::split_by_delim(line, ',', tokens, false);
          // skip header line
          if(strcmp(tokens[hh_label], "hh_id") != 0 && strcmp(tokens[hh_label], "sp_id") != 0) {
            char s[80];

            sprintf(s, "%s", tokens[hh_label]);
            string hh_label_str(s);
            sprintf(s, "%s", tokens[hospital_label]);
            string hosp_label_str(s);
            this->hh_label_hosp_label_map.insert(std::pair<string, string>(hh_label_str, hosp_label_str));
          }
          tokens.clear();
        }
        fclose(hospital_household_map_fp);
      }
    }
  }

  //added for cbsa
  if(strcmp(Global::MSA_code, "none") != 0) {
    // msa param overrides other locations, used to populate the synthetic_population_id
    // get fips(s) from msa code
    char msaline_string[FRED_STRING_SIZE];
    char pop_id[FRED_FIPS_LIST_SIZE];
    char* msaline;
    char* cbsa;
    char* msa;
    char* fips;
    int msafound = 0;
    int msaLength = strlen(Global::MSA_code);
    if(msaLength == 5) {
      FILE* msafp = Utils::fred_open_file(Place_List::MSA_file);
      if(msafp == NULL) {
        Utils::fred_abort("msa file |%s| NOT FOUND\n", Place_List::MSA_file);
      }
      while(fgets(msaline_string, FRED_STRING_SIZE - 1, msafp) != NULL) {
        msaline = msaline_string;
        cbsa = strsep(&msaline, "\t");
        msa = strsep(&msaline, "\n");
        if(strcmp(Global::MSA_code, cbsa) == 0) {
          msafound = 1;
          break;
        }
      }
      fclose(msafp);
      if(msafound) {
        Utils::fred_log("FOUND FIPS = |%s msa | for cbsa = |%s|\n", msa, cbsa);
        int first = 1;
        while((fips = strsep(&msa, " ")) != NULL) {
          if(first == 1) { //first one uses strcpy to start string
            strcpy(pop_id, Global::Synthetic_population_version);
            strcat(pop_id, "_");
            strcat(pop_id, fips);
            first++;
          } else {
            strcat(pop_id, " ");
            strcat(pop_id, Global::Synthetic_population_version);
            strcat(pop_id, "_");
            strcat(pop_id, fips);
          }
        }
        sprintf(Global::Synthetic_population_id, "%s", pop_id);
      } else {
        Utils::fred_abort("Sorry, could not find fips for MSA = |%s|\n", Global::MSA_code);
      }
    }
  } else if(strcmp(Global::FIPS_code, "none") != 0) {

    // fips param overrides the synthetic_population_id

    // get population_id from fips
    char line_string[FRED_STRING_SIZE];
    char* line;
    char* city;
    char* state;
    char* county;
    char* fips;
    int found = 0;
    int fipsLength = strlen(Global::FIPS_code);
    if(fipsLength == 5) {
      FILE* fp = Utils::fred_open_file(Place_List::Counties_file);
      if(fp == NULL) {
        Utils::fred_abort("counties file |%s| NOT FOUND\n", Place_List::Counties_file);
      }
      while(fgets(line_string, FRED_STRING_SIZE - 1, fp) != NULL) {
        line = line_string;
        city = strsep(&line, "\t");
        state = strsep(&line, "\t");
        county = strsep(&line, "\t");
        fips = strsep(&line, "\n");
        // printf("city = |%s| state = |%s| county = |%s| fips = |%s|\n",
        // city,state,county,fips);
        if(strcmp(Global::FIPS_code, fips) == 0) {
          found = 1;
          break;
        }
      }
      fclose(fp);
      if(found) {
        Utils::fred_log("FOUND a county = |%s County %s| for fips = |%s|\n", county, state, fips);
        sprintf(Global::Synthetic_population_id, "%s_%s", Global::Synthetic_population_version, fips);
      } else {
        Utils::fred_abort("Sorry, could not find a county for fips = |%s|\n", Global::FIPS_code);
      }
    } else if(fipsLength == 2) {
      // get population_id from state
      char line_string[FRED_STRING_SIZE];
      char* line;
      char* abbrev;
      char* state;
      char* fips;
      int found = 0;
      FILE* fp = Utils::fred_open_file(Place_List::States_file);
      if(fp == NULL) {
        Utils::fred_abort("states file |%s| NOT FOUND\n", Place_List::States_file);
      }
      while(fgets(line_string, FRED_STRING_SIZE - 1, fp) != NULL) {
        line = line_string;
        fips = strsep(&line, "\t");
        abbrev = strsep(&line, "\t");
        state = strsep(&line, "\n");
        if(strcmp(Global::FIPS_code, fips) == 0) {
          found = 1;
          break;
        }
      }
      fclose(fp);
      if(found) {
        Utils::fred_log("FOUND state = |%s| state_abbrev = |%s| fips = |%s|\n", state, abbrev, fips);
        sprintf(Global::Synthetic_population_id, "%s_%s", Global::Synthetic_population_version, fips);
      } else {
        Utils::fred_abort("Sorry, could not find state called |%s|\n", Global::US_state);
      }
    } else {
      Utils::fred_abort(
			"FRED keyword fips only supports 2 digits (for states) and 5 digits (for counties), you specified %s",
			Global::FIPS_code);
    }
  } else if(strcmp(Global::City, "none") != 0) {

    // city param overrides the synthetic_population_id

    // delete any commas and periods
    Utils::delete_char(Global::City, ',', FRED_STRING_SIZE);
    Utils::delete_char(Global::City, '.', FRED_STRING_SIZE);

    // replace white space characters with a single space
    Utils::normalize_white_space(Global::City);

    // get population_id from city
    char city_state[FRED_STRING_SIZE];
    char line_string[FRED_STRING_SIZE];
    char* line;
    char* city;
    char* state;
    char* county;
    char* fips;
    int found = 0;
    FILE* fp = Utils::fred_open_file(Place_List::Counties_file);
    if(fp == NULL) {
      Utils::fred_abort("counties file |%s| NOT FOUND\n", Place_List::Counties_file);
    }
    while(fgets(line_string, FRED_STRING_SIZE - 1, fp) != NULL) {
      line = line_string;
      city = strsep(&line, "\t");
      state = strsep(&line, "\t");
      county = strsep(&line, "\t");
      fips = strsep(&line, "\n");
      // printf("city = |%s| state = |%s| county = |%s| fips = |%s|\n",
      // city,state,county,fips);
      sprintf(city_state, "%s %s", city, state);
      if(strcmp(Global::City, city_state) == 0) {
        found = 1;
        break;
      }
    }
    fclose(fp);
    if(found) {
      Utils::fred_log("FOUND a county for city = |%s| county = |%s County %s| and fips = |%s|\n", Global::City, county,
		      state, fips);
      sprintf(Global::Synthetic_population_id, "%s_%s", Global::Synthetic_population_version, fips);
    } else {
      Utils::fred_abort("Sorry, could not find a county for city = |%s|\n", Global::City);
    }
  } else if(strcmp(Global::County, "none") != 0) {

    // county param overrides the synthetic_population_id

    // delete any commas and periods
    Utils::delete_char(Global::County, ',', FRED_STRING_SIZE);
    Utils::delete_char(Global::County, '.', FRED_STRING_SIZE);

    // replace white space characters with a single space
    Utils::normalize_white_space(Global::County);

    // get population_id from county
    char county_state[FRED_STRING_SIZE];
    char line_string[FRED_STRING_SIZE];
    char* line;
    char* city;
    char* state;
    char* county;
    char* fips;
    int found = 0;
    FILE* fp = Utils::fred_open_file(Place_List::Counties_file);
    if(fp == NULL) {
      Utils::fred_abort("counties file |%s| NOT FOUND\n", Place_List::Counties_file);
    }
    while(fgets(line_string, FRED_STRING_SIZE - 1, fp) != NULL) {
      line = line_string;
      city = strsep(&line, "\t");
      state = strsep(&line, "\t");
      county = strsep(&line, "\t");
      fips = strsep(&line, "\n");
      // printf("city = |%s| state = |%s| county = |%s| fips = |%s|\n",
      // city,state,county,fips);
      sprintf(county_state, "%s County %s", county, state);
      if(strcmp(Global::County, county_state) == 0) {
        found = 1;
        break;
      }
    }
    fclose(fp);
    if(found) {
      Utils::fred_log("FOUND county = |%s| fips = |%s|\n", county_state, fips);
      sprintf(Global::Synthetic_population_id, "%s_%s", Global::Synthetic_population_version, fips);
    } else {
      Utils::fred_abort("Sorry, could not find county called |%s|\n", Global::County);
    }
  } else if(strcmp(Global::US_state, "none") != 0) {

    // state param overrides the synthetic_population_id

    // delete any commas and periods
    Utils::delete_char(Global::US_state, ',', FRED_STRING_SIZE);
    Utils::delete_char(Global::US_state, '.', FRED_STRING_SIZE);

    // replace white space characters with a single space
    Utils::normalize_white_space(Global::US_state);

    // get population_id from state
    char line_string[FRED_STRING_SIZE];
    char* line;
    char* abbrev;
    char* state;
    char* fips;
    int found = 0;
    FILE* fp = Utils::fred_open_file(Place_List::States_file);
    if(fp == NULL) {
      Utils::fred_abort("states file |%s| NOT FOUND\n", Place_List::States_file);
    }
    while(fgets(line_string, FRED_STRING_SIZE - 1, fp) != NULL) {
      line = line_string;
      fips = strsep(&line, "\t");
      abbrev = strsep(&line, "\t");
      state = strsep(&line, "\n");
      if(strcmp(Global::US_state, abbrev) == 0 || strcmp(Global::US_state, state) == 0) {
        found = 1;
        break;
      }
    }
    fclose(fp);
    if(found) {
      Utils::fred_log("FOUND state = |%s| state_abbrev = |%s| fips = |%s|\n", state, abbrev, fips);
      sprintf(Global::Synthetic_population_id, "%s_%s", Global::Synthetic_population_version, fips);
    } else {
      Utils::fred_abort("Sorry, could not find state called |%s|\n", Global::US_state);
    }
  }
}

void Place_List::read_all_places(const std::vector<Utils::Tokens> &Demes) {

  for (int i = 0; i < Demes.size(); i++) {
    FRED_VERBOSE(0, "read_all_places: Demes[%d][0] = %s\n", i, Demes[i][0]);
  }

  // clear the vectors and maps
  this->households.clear();
  this->neighborhoods.clear();
  this->schools.clear();
  this->workplaces.clear();
  this->hospitals.clear();
  this->counties.clear();
  this->census_tracts.clear();
  this->fips_to_county_map.clear();
  this->fips_to_census_tract_map.clear();
  this->hosp_label_hosp_id_map.clear();
  this->hh_label_hosp_label_map.clear();

  // store the number of demes as member variable
  set_number_of_demes(Demes.size());

  // to compute the region's bounding box
  this->min_lat = this->min_lon = 999;
  this->max_lat = this->max_lon = -999;

  // only one population directory allowed
  const char* pop_dir = Global::Synthetic_population_directory;

  // need to have at least one deme
  assert(Demes.size() > 0);
  assert(Demes.size() <= std::numeric_limits<unsigned char>::max());

  // and each deme must contain at least one synthetic population id
  for(int d = 0; d < Demes.size(); ++d) {
    for (int j = 0; j < Demes.size(); j++) {
      FRED_VERBOSE(0, "before read_places: Demes[%d][0] = %s\n", j, Demes[j][0]);
    }
    FRED_STATUS(0, "Reading Places for Deme %d  pop_id = %s:\n", d, Demes[d][0]);
    assert(Demes[d].size() > 0);
    for(int i = 0; i < Demes[d].size(); ++i) {
      read_places(pop_dir, Demes[d][i], d);
    }
    for (int j = 0; j < Demes.size(); j++) {
      FRED_VERBOSE(0, "after read_places: Demes[%d][0] = %s\n", j, Demes[j][0]);
    }
  }

  for(int i = 0; i < this->counties.size(); ++i) {
    int fips = this->counties[i]->get_fips();
    FRED_VERBOSE(0, "COUNTIES[%d] = %05d\n", i, fips);
  }
  for(int i = 0; i < this->census_tracts.size(); ++i) {
    long int fips = this->census_tracts[i]->get_fips();
    FRED_VERBOSE(0, "CENSUS_TRACTS[%d] = %011ld\n", i, fips);
  }

  FRED_STATUS(0, "finished reading %d locations, now creating additional FRED locations\n", next_place_id);

  if(Global::Use_Mean_Latitude) {
    // Make projection based on the location file.
    fred::geo mean_lat = (min_lat + max_lat) / 2.0;
    Geo::set_km_per_degree(mean_lat);
    Utils::fred_log("min_lat: %f  max_lat: %f  mean_lat: %f\n", min_lat, max_lat, mean_lat);
  } else {
    // DEFAULT: Use mean US latitude (see Geo.cc)
    Utils::fred_log("min_lat: %f  max_lat: %f\n", min_lat, max_lat);
  }

  // create geographical grids
  Global::Simulation_Region = new Regional_Layer(min_lon, min_lat, max_lon, max_lat);

  // Initialize global seasonality object
  if(Global::Enable_Seasonality) {
    Global::Clim = new Seasonality(Global::Simulation_Region);
  }

  // layer containing neighborhoods
  Global::Neighborhoods = new Neighborhood_Layer();

  // add households to the Neighborhoods Layer
  FRED_VERBOSE(0, "adding %d households to neighborhoods\n", this->households.size());
  for(int i = 0; i < this->households.size(); ++i) {
    Household* h = this->get_household(i);
    int row = Global::Neighborhoods->get_row(h->get_latitude());
    int col = Global::Neighborhoods->get_col(h->get_longitude());
    Neighborhood_Patch* patch = Global::Neighborhoods->get_patch(row, col);

    FRED_CONDITIONAL_VERBOSE(0, patch == NULL, "Help: household %d has bad patch,  lat = %f  lon = %f\n", h->get_id(),
			     h->get_latitude(), h->get_longitude());

    assert(patch != NULL);

    patch->add_household(h);
    h->set_patch(patch);
  }

  int number_of_neighborhoods = Global::Neighborhoods->get_number_of_neighborhoods();

  // Neighborhood_Layer::setup call Neighborhood_Patch::make_neighborhood
  Global::Neighborhoods->setup();
  FRED_VERBOSE(0, "Created %d neighborhoods\n", this->neighborhoods.size());

  // add workplaces to Regional grid (for worker reassignment)
  int number_places = static_cast<int>(this->workplaces.size());
  for(int p = 0; p < number_places; ++p) {
    Global::Simulation_Region->add_workplace(this->workplaces[p]);
  }

  // add hospitals to Regional grid (for household hospital assignment)
  number_places = static_cast<int>(this->hospitals.size());
  for(int p = 0; p < number_places; ++p) {
    // printf("ADD HOSP %d %s\n", p, this->hospitals[p]->get_label());
    Global::Simulation_Region->add_hospital(this->hospitals[p]);
  }

  this->load_completed = true;
  FRED_STATUS(0, "read places finished: Places = %d\n", (int) places.size());
}

void Place_List::read_places(const char* pop_dir, const char* pop_id, unsigned char deme_id) {

  FRED_STATUS(0, "read places entered\n", "");

  char location_file[FRED_STRING_SIZE];
  char temp_file[80];
  if(getenv("SCRATCH_RAMDISK") != NULL) {
    sprintf(temp_file, "%s/temp_file-%d-%d", getenv("SCRATCH_RAMDISK"), (int)getpid(), Global::Simulation_run_number);
  } else {
    sprintf(temp_file, "./temp_file-%d-%d", (int)getpid(), Global::Simulation_run_number);
  }

  // record the actual synthetic population in the log file
  Utils::fred_log("POPULATION_FILE: %s/%s\n", pop_dir, pop_id);

  // read household locations
  sprintf(location_file, "%s/%s/%s_synth_households.txt", pop_dir, pop_id, pop_id);
  if(Place_List::Enable_copy_files) {
    std::ifstream  src(location_file, std::ios::binary);
    std::ofstream  dst(temp_file,   std::ios::binary);
    dst << src.rdbuf();
    strcpy(location_file, temp_file);
  }
  read_household_file(deme_id, location_file);
  Utils::fred_print_lap_time("Places.read_household_file");

  // log county info
  for(int i = 0; i < this->counties.size(); ++i) {
    fprintf(Global::Statusfp, "COUNTIES[%d] = %05d\n", i, this->counties[i]->get_fips());
  }

  // read school locations
  sprintf(location_file, "%s/%s/%s_schools.txt", pop_dir, pop_id, pop_id);
  read_school_file(deme_id, location_file);

  // read workplace locations
  sprintf(location_file, "%s/%s/%s_workplaces.txt", pop_dir, pop_id, pop_id);
  read_workplace_file(deme_id, location_file);

  // read hospital locations
  if(Global::Enable_Hospitals) {
    sprintf(location_file, "%s/%s/%s_hospitals.txt", pop_dir, pop_id, pop_id);
    read_hospital_file(deme_id, location_file);
  }

  if(Global::Enable_Group_Quarters) {
    // read group quarters locations (a new workplace and household is created 
    // for each group quarters)
    sprintf(location_file, "%s/%s/%s_synth_gq.txt", pop_dir, pop_id, pop_id);
    read_group_quarters_file(deme_id, location_file);
    Utils::fred_print_lap_time("Places.read_group_quarters_file");

    // log county info
    fprintf(Global::Statusfp, "COUNTIES AFTER READING GQ\n");
    for(int i = 0; i < this->counties.size(); ++i) {
      fprintf(Global::Statusfp, "COUNTIES[%d] = %05d\n", i, this->counties[i]->get_fips());
    }
  }
  FRED_STATUS(0, "read places finished\n", "");
}


void Place_List::read_household_file(unsigned char deme_id, char* location_file) {
  // location of fields in input file
  int id_field = 0;
  int fips_field = 2;
  int lat_field = 7;
  int lon_field = 8;
  int race_field = 3;
  int income_field = 4;

  // data to fill in from input file
  char place_type = Place::TYPE_HOUSEHOLD;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[80];
  char fips_str[12];
  long int census_tract_fips = 0;
  int county_fips;
  double lat;
  double lon;
  int race;
  int income;

  char line_str[10*FRED_STRING_SIZE];
  Utils::Tokens tokens;
  FILE* fp = Utils::fred_open_file(location_file);

  for(char* line = line_str; fgets(line, 10*FRED_STRING_SIZE, fp); line = line_str) {
    // printf("%s\n",line); fflush(stdout);
    tokens.clear();
    tokens = Utils::split_by_delim(line, ',', tokens, false);

    // skip header line
    if(strcmp(tokens[id_field], "sp_id") == 0) {
      continue;
    }

    // place label
    sprintf(label, "%c%s", place_type, tokens[id_field]);

    // lat/lon
    sscanf(tokens[lat_field], "%lf", &lat); 
    sscanf(tokens[lon_field], "%lf", &lon); 
    update_geo_boundaries(lat, lon);

    // census tract
    // use the first eleven (state and county + six) digits of fips_field to get the census tract
    // e.g 090091846001 StateCo = 09009, 184600 is the census tract, throw away the 1
    strncpy(fips_str, tokens[fips_field], 11);
    fips_str[11] = '\0';
    sscanf(fips_str, "%ld", &census_tract_fips);
    Household* place = static_cast<Household*>(add_place(label, place_type, place_subtype, lon, lat, census_tract_fips));
    
    // if this is a new census_tracts fips code, create a Census_tract object
    std::map<long int,int>::iterator itr_tract;
    itr_tract = this->fips_to_census_tract_map.find(census_tract_fips);
    if (itr_tract == this->fips_to_census_tract_map.end()) {
      Census_Tract* new_census_tract = new Census_Tract(census_tract_fips);
      this->census_tracts.push_back(new_census_tract);
      this->fips_to_census_tract_map[census_tract_fips] = this->census_tracts.size() - 1;
    }

    // add the household to the census_tract's list
    Census_Tract* census_tract = get_census_tract(census_tract_fips);
    census_tract->add_household(place);

    // county fips code
    // use the first five digits of fips_field to get the county fips code
    strncpy(fips_str, tokens[fips_field], 5);
    fips_str[5] = '\0';
    sscanf(fips_str, "%d", &county_fips);

    // if this is a new county fips code, create a County object
    std::map<int,int>::iterator itr;
    itr = this->fips_to_county_map.find(county_fips);
    if (itr == this->fips_to_county_map.end()) {
      County* new_county = new County(county_fips);
      this->counties.push_back(new_county);
      this->fips_to_county_map[county_fips] = this->counties.size() - 1;
    }

    // add the household to the county list
    County* county = get_county(county_fips);
    county->add_household(place);
      
    // printf("county = %d census_tract = %ld\n", county_fips, census_tract_fips);

    // household race and income
    sscanf(tokens[race_field], "%d", &race); 
    place->set_household_race(race);
    sscanf(tokens[income_field], "%d", &income); 
    place->set_household_income(income);
  }
  fclose(fp);
}

void Place_List::read_workplace_file(unsigned char deme_id, char* location_file) {
  // location of fields in input file
  int id_field = 0;
  int lat_field = 2;
  int lon_field = 3;

  // data to fill in from input file
  char place_type = Place::TYPE_WORKPLACE;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[80];
  double lat;
  double lon;

  char line_str[10*FRED_STRING_SIZE];
  Utils::Tokens tokens;
  FILE* fp = Utils::fred_open_file(location_file);

  for(char* line = line_str; fgets(line, 10*FRED_STRING_SIZE, fp); line = line_str) {

    tokens.clear();
    tokens = Utils::split_by_delim(line, ',', tokens, false);

    // skip header line
    if(strcmp(tokens[id_field], "sp_id") == 0) {
      continue;
    }

    // place label
    sprintf(label, "%c%s", place_type, tokens[id_field]);

    // lat/lon
    sscanf(tokens[lat_field], "%lf", &lat); 
    sscanf(tokens[lon_field], "%lf", &lon); 

    Place* place = add_place(label, place_type, place_subtype, lon, lat, 0);
  }
  fclose(fp);
}

void Place_List::read_hospital_file(unsigned char deme_id, char* location_file) {

  // location of fields in input file
  int id_field = 0;
  int workers_field = 6;
  int physicians_field = 7;
  int beds_field = 8;
  int lat_field = 9;
  int lon_field = 10;

  // data to fill in from input file
  char place_type = Place::TYPE_HOSPITAL;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[80];
  double lat;
  double lon;
  int workers;
  int physicians;
  int beds;

  char line_str[10*FRED_STRING_SIZE];
  Utils::Tokens tokens;
  FILE* fp = Utils::fred_open_file(location_file);

  int new_hospitals = 0;
  for(char* line = line_str; fgets(line, 10*FRED_STRING_SIZE, fp); line = line_str) {

    tokens.clear();
    tokens = Utils::split_by_delim(line, ',', tokens, false);

    // skip header line
    if(strcmp(tokens[id_field], "sp_id") == 0) {
      continue;
    }

    // printf("READ HOSP %s", line);

    // place label
    sprintf(label, "%c%s", place_type, tokens[id_field]);

    // lat/lon
    sscanf(tokens[lat_field], "%lf", &lat); 
    sscanf(tokens[lon_field], "%lf", &lon); 
    update_geo_boundaries(lat, lon);

    // workers
    sscanf(tokens[workers_field], "%d", &workers); 

    // physicians
    sscanf(tokens[physicians_field], "%d", &physicians); 

    // beds
    sscanf(tokens[beds_field], "%d", &beds); 

    Hospital* place = static_cast<Hospital*>(add_place(label, place_type, place_subtype, lon, lat, 0));
    place->set_employee_count(workers);
    place->set_physician_count(physicians);
    place->set_bed_count(beds);

    string hosp_label_str(label);
    int hosp_id = this->hospitals.size() - 1;
    this->hosp_label_hosp_id_map.insert(std::pair<string, int>(hosp_label_str, hosp_id));
    new_hospitals++;
    // printf("READ HOSP %s hosp_id %d\n", place->get_label(), hosp_id);
  }
  fclose(fp);
  FRED_VERBOSE(0, "read_hospital_file: found %d hospitals\n", new_hospitals);
}


void Place_List::read_school_file(unsigned char deme_id, char* location_file) {
  // location of fields in input file
  int id_field = 0;
  int fips_field = 17;
  int lat_field = 14;
  int lon_field = 15;

  // place data to fill in from input file
  char place_type = Place::TYPE_SCHOOL;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[80];
  long int census_tract_fips = 0;
  double lat;
  double lon;

  char county_fips_str[8];
  char line_str[10*FRED_STRING_SIZE];
  Utils::Tokens tokens;
  FILE* fp = Utils::fred_open_file(location_file);

  for(char* line = line_str; fgets(line, 10*FRED_STRING_SIZE, fp); line = line_str) {

    tokens.clear();
    tokens = Utils::split_by_delim(line, ',', tokens, false);

    // skip header line
    if(strcmp(tokens[id_field], "sp_id") == 0) {
      continue;
    }

    // place label
    sprintf(label, "%c%s", place_type, tokens[id_field]);

    // lat/lon
    sscanf(tokens[lat_field], "%lf", &lat); 
    sscanf(tokens[lon_field], "%lf", &lon); 

    // census tract fips code
    strncpy(county_fips_str, tokens[fips_field], 5);
    county_fips_str[5] = '\0';
    sscanf(county_fips_str, "%ld", &census_tract_fips);
    census_tract_fips *= 1000000;
    Place* place = add_place(label, place_type, place_subtype, lon, lat, census_tract_fips);
  }
  fclose(fp);
}


void Place_List::read_group_quarters_file(unsigned char deme_id, char* location_file) {

  // location of fields in input file
  int id_field = 0;
  int type_field = 1;
  int size_field = 2;
  int fips_field = 3;
  int lat_field = 4;
  int lon_field = 5;

  // data to fill in from input file
  char place_type = Place::TYPE_HOUSEHOLD;
  char place_subtype = Place::SUBTYPE_NONE;
  char label[80];
  char fips_str[12];
  long int census_tract_fips = 0;
  int county_fips;
  double lat;
  double lon;
  int capacity;

  char line_str[10*FRED_STRING_SIZE];
  Utils::Tokens tokens;
  FILE* fp = Utils::fred_open_file(location_file);

  for(char* line = line_str; fgets(line, 10*FRED_STRING_SIZE, fp); line = line_str) {
    tokens.clear();
    tokens = Utils::split_by_delim(line, ',', tokens, false);

    // skip header line
    if(strcmp(tokens[id_field], "sp_id") == 0) {
      continue;
    }

    // lat/lon
    sscanf(tokens[lat_field], "%lf", &lat); 
    sscanf(tokens[lon_field], "%lf", &lon); 
    update_geo_boundaries(lat, lon);

    // census tract
    // use the first eleven (state and county + six) digits of fips_field to get the census tract
    // e.g 090091846001 StateCo = 09009, 184600 is the census tract, throw away the 1
    strncpy(fips_str, tokens[fips_field], 11);
    fips_str[11] = '\0';
    sscanf(fips_str, "%ld", &census_tract_fips);

    // if this is a new census_tracts fips code, create a Census_tract object
    std::map<long int,int>::iterator itr_tract;
    itr_tract = this->fips_to_census_tract_map.find(census_tract_fips);
    if (itr_tract == this->fips_to_census_tract_map.end()) {
      Census_Tract* new_census_tract = new Census_Tract(census_tract_fips);
      this->census_tracts.push_back(new_census_tract);
      this->fips_to_census_tract_map[census_tract_fips] = this->census_tracts.size() - 1;
    }

    // county fips code
    // use the first five digits of fips_field to get the county fips code
    strncpy(fips_str, tokens[fips_field], 5);
    fips_str[5] = '\0';
    sscanf(fips_str, "%d", &county_fips);

    // if this is a new county fips code, create a County object
    std::map<int,int>::iterator itr;
    itr = this->fips_to_county_map.find(county_fips);
    if (itr == this->fips_to_county_map.end()) {
      County* new_county = new County(county_fips);
      this->counties.push_back(new_county);
      this->fips_to_county_map[county_fips] = this->counties.size() - 1;
    }

    // size
    sscanf(tokens[size_field], "%d", &capacity); 

    // set number of units and subtype for this group quarters
    int number_of_units = 0;
    if(strcmp(tokens[type_field], "C") == 0) {
      number_of_units = capacity / Place_List::College_dorm_mean_size;
      place_subtype = Place::SUBTYPE_COLLEGE;
    }
    if(strcmp(tokens[type_field], "M") == 0) {
      number_of_units = capacity / Place_List::Military_barracks_mean_size;
      place_subtype = Place::SUBTYPE_MILITARY_BASE;
    }
    if(strcmp(tokens[type_field], "P") == 0) {
      number_of_units = capacity / Place_List::Prison_cell_mean_size;
      place_subtype = Place::SUBTYPE_PRISON;
    }
    if(strcmp(tokens[type_field], "N") == 0) {
      number_of_units = capacity / Place_List::Nursing_home_room_mean_size;
      place_subtype = Place::SUBTYPE_NURSING_HOME;
    }
    if(number_of_units == 0) {
      number_of_units = 1;
    }

    // add a workplace for this group quarters
    place_type = Place::TYPE_WORKPLACE;
    sprintf(label, "%c%s", place_type, tokens[id_field]);
    FRED_VERBOSE(0, "Adding GQ Workplace %s subtype %c\n", label, place_subtype);
    Place* workplace = add_place(label, place_type, place_subtype, lon, lat, census_tract_fips);
    
    // add as household
    place_type = Place::TYPE_HOUSEHOLD;
    sprintf(label, "%c%s", place_type, tokens[id_field]);

    FRED_VERBOSE(0, "Adding GQ Household %s subtype %c\n", label, place_subtype);
    Household *place = static_cast<Household *>(add_place(label, place_type, place_subtype, lon, lat, census_tract_fips));
    place->set_group_quarters_units(number_of_units);
    place->set_group_quarters_workplace(workplace);
    
    // add the household to the census_tract's list
    Census_Tract* census_tract = get_census_tract(census_tract_fips);
    census_tract->add_household(place);

    // add the household to the county list
    County* county = get_county(county_fips);
    county->add_household(place);
      
    // generate additional household units associated with this group quarters
    for(int i = 1; i < number_of_units; ++i) {
      sprintf(label, "%c%s-%03d", place_type, tokens[id_field], i);
      Household *place = static_cast<Household *>(add_place(label, place_type, place_subtype, lon, lat, census_tract_fips));
      FRED_VERBOSE(0, "Adding GQ Household %s subtype %c out of %d units\n", label, place_subtype, number_of_units);

      // add the household to the census_tract's list
      Census_Tract* census_tract = get_census_tract(census_tract_fips);
      census_tract->add_household(place);

      // add the household to the county list
      County* county = get_county(county_fips);
      county->add_household(place);
    }
  }
  fclose(fp);
}


void Place_List::setup_counties() {
  // set each county's school and workplace attendance probabilities
  for(int i = 0; i < this->counties.size(); ++i) {
    this->counties[i]->setup();
  }
}


void Place_List::setup_census_tracts() {
  // set each census tract's school and workplace attendance probabilities
  for(int i = 0; i < this->census_tracts.size(); ++i) {
    this->census_tracts[i]->setup();
  }
}


void Place_List::prepare() {

  FRED_STATUS(0, "prepare places entered\n", "");

  int number_places = places.size();
  for(int p = 0; p < number_places; ++p) {
    this->places[p]->prepare();
  }
  Global::Neighborhoods->prepare();

  // create lists of school by grade
  int number_of_schools = this->schools.size();
  for(int p = 0; p < number_of_schools; ++p) {
    School* school = get_school(p);
    for(int grade = 0; grade < GRADES; ++grade) {
      if(school->get_orig_students_in_grade(grade) > 0) {
	this->schools_by_grade[grade].push_back(school);
      }
    }
  }

  if(Global::Verbose > 1) {
    // check the schools by grade lists
    printf("\n");
    for(int grade = 0; grade < GRADES; ++grade) {
      int size = this->schools_by_grade[grade].size();
      printf("GRADE = %d SCHOOLS = %d: ", grade, size);
      for(int i = 0; i < size; ++i) {
        printf("%s ", this->schools_by_grade[grade][i]->get_label());
      }
      printf("\n");
    }
    printf("\n");
  }
  if (Global::Verbose > 0) {
    print_status_of_schools(0);
  }

  // add household list to visualization layer if needed
  /*
    int num_households = this->households.size();
    if(Global::Enable_Visualization_Layer) {
    for(int i = 0; i < num_households; ++i) {
    Household* h = this->get_household(i);
    // Global::Visualization->add_household(h);
    }
    }

    // print out household locations to visualization directory
    char filename[256];
    sprintf(filename, "%s/households.txt", Global::Visualization_directory);
    FILE* fp = fopen(filename, "w");
    for(int i = 0; i < num_households; ++i) {
    Household* h = get_household(i);
    fprintf(fp, "%f %f %3d %s\n", h->get_latitude(), h->get_longitude(), h->get_size(), h->get_label());
    }
    fclose(fp);
  */

  // add list of counties to visualization data directory
  char filename[256];
  sprintf(filename, "%s/VIS/COUNTIES", Global::Simulation_directory);
  FILE* fp = fopen(filename, "w");
  for(int i = 0; i < this->counties.size(); ++i) {
    fprintf(fp, "%05d\n", this->counties[i]->get_fips());
  }
  fclose(fp);

  // add list of census_tracts to visualization data directory
  sprintf(filename, "%s/VIS/CENSUS_TRACTS", Global::Simulation_directory);
  fp = fopen(filename, "w");
  for(int i = 0; i < this->census_tracts.size(); ++i) {
    long int fips = this->census_tracts[i]->get_fips();
    fprintf(fp, "%011ld\n", fips);
  }
  fclose(fp);

}

void Place_List::print_status_of_schools(int day) {
  int students_per_grade[GRADES];
  for(int i = 0; i < GRADES; ++i) {
    students_per_grade[i] = 0;
  }

  int number_of_schools = this->schools.size();
  for(int p = 0; p < number_of_schools; ++p) {
    School *school = get_school(p);
    for(int grade = 0; grade < GRADES; ++grade) {
      int total = school->get_orig_number_of_students();
      int orig = school->get_orig_students_in_grade(grade);
      int now = school->get_students_in_grade(grade);
      students_per_grade[grade] += now;
      if(0 && total > 1500 && orig > 0) {
	printf("%s GRADE %d ORIG %d NOW %d DIFF %d\n", school->get_label(), grade,
	       school->get_orig_students_in_grade(grade),
	       school->get_students_in_grade(grade),
	       school->get_students_in_grade(grade)
	       - school->get_orig_students_in_grade(grade));
      }
    }
  }

  int year = day / 365;
  // char filename[256];
  // sprintf(filename, "students.%d", year);
  // FILE *fp = fopen(filename,"w");
  int total_students = 0;
  for(int i = 0; i < GRADES; ++i) {
    // fprintf(fp, "%d %d\n", i,students_per_grade[i]);
    printf("YEAR %d GRADE %d STUDENTS %d\n", year, i, students_per_grade[i]);
    total_students += students_per_grade[i];
  }
  // fclose(fp);
  printf("YEAR %d TOTAL_STUDENTS %d\n", year, total_students);
}

void Place_List::update(int day) {

  FRED_STATUS(1, "update places entered\n", "");

  /* debugging:
  int num_households = this->households.size();
  int dorms = 0;
  for(int p = 0; p < num_households; ++p) {
    Household* house = this->get_household(p);
    if (house->is_college_dorm()) {
      dorms++;
    }
  }
  FRED_VERBOSE(0, "update day %d dorms %d hh %d\n", day, dorms, num_households);
  */

  if(Global::Enable_Seasonality) {
    Global::Clim->update(day);
  }

  if(Global::Enable_Vector_Transmission) {
    int number_places = this->places.size();
    for(int p = 0; p < number_places; ++p) {
      Place* place = this->places[p];
      place->update_vector_population(day);
    }
  }

  if(Global::Enable_HAZEL) {
    int number_places = this->places.size();
    for(int p = 0; p < number_places; ++p) {
      Place* place = this->places[p];

      if(place->is_hospital()) {
        Hospital* temp_hosp = static_cast<Hospital*>(place);
        temp_hosp->reset_current_daily_patient_count();
      }

      if(place->is_household()) {
        Household* temp_hh = static_cast<Household*>(place);
        temp_hh->reset_healthcare_info();
      }
    }
  }

  FRED_STATUS(1, "update places finished\n", "");
}

void Place_List::setup_household_childcare() {
  assert(this->is_load_completed());
  assert(Global::Pop.is_load_completed());
  if(Global::Report_Childhood_Presenteeism) {
    int number_places = this->households.size();
    for(int p = 0; p < number_places; ++p) {
      Household* hh = get_household(p);
      hh->prepare_person_childcare_sickleave_map();
    }
  }
}

void Place_List::setup_school_income_quartile_pop_sizes() {
  assert(this->is_load_completed());
  assert(Global::Pop.is_load_completed());
  if(Global::Report_Childhood_Presenteeism) {
    int number_places = this->schools.size();
    for(int p = 0; p < number_places; ++p) {
      School* school = get_school(p);
      school->prepare_income_quartile_pop_size();
    }
  }
}

void Place_List::setup_household_income_quartile_sick_days() {
  assert(this->is_load_completed());
  assert(Global::Pop.is_load_completed());
  if(Global::Report_Childhood_Presenteeism) {
    typedef std::multimap<double, Household*> HouseholdMultiMapT;

    HouseholdMultiMapT* household_income_hh_mm = new HouseholdMultiMapT();
    int number_households = this->households.size();
    for(int p = 0; p < number_households; ++p) {
      Household* hh = get_household(p);
      double hh_income = hh->get_household_income();
      std::pair<double, Household*> my_insert(hh_income, hh);
      household_income_hh_mm->insert(my_insert);
    }

    int total = static_cast<int>(household_income_hh_mm->size());
    int q1 = total / 4;
    int q2 = q1 * 2;
    int q3 = q1 * 3;

    FRED_STATUS(0, "\nPROBABILITY WORKERS HAVE PAID SICK DAYS BY HOUSEHOLD INCOME QUARTILE:\n");
    double q1_sick_leave = 0.0;
    double q1_count = 0.0;
    double q2_sick_leave = 0.0;
    double q2_count = 0.0;
    double q3_sick_leave = 0.0;
    double q3_count = 0.0;
    double q4_sick_leave = 0.0;
    double q4_count = 0.0;
    int counter = 0;
    for(HouseholdMultiMapT::iterator itr = household_income_hh_mm->begin(); itr != household_income_hh_mm->end(); ++itr) {
      double hh_sick_leave_total = 0.0;
      double hh_employee_total = 0.0;

      for(int i = 0; i < static_cast<int>((*itr).second->enrollees.size()); ++i) {
        Person* per = (*itr).second->enrollees[i];
        if(per->is_adult() && !per->is_student()
	   && (per->get_activities()->is_teacher() || per->get_activities()->get_profile() == WORKER_PROFILE
	       || per->get_activities()->get_profile() == WEEKEND_WORKER_PROFILE)) {
          hh_sick_leave_total += (per->get_activities()->is_sick_leave_available() ? 1.0 : 0.0);
          hh_employee_total += 1.0;
        }
      }

      if(counter < q1) {
        (*itr).second->set_income_quartile(Global::Q1);
        q1_sick_leave += hh_sick_leave_total;
        q1_count += hh_employee_total;
      } else if(counter < q2) {
        (*itr).second->set_income_quartile(Global::Q2);
        q2_sick_leave += hh_sick_leave_total;
        q2_count += hh_employee_total;
      } else if(counter < q3) {
        (*itr).second->set_income_quartile(Global::Q3);
        q3_sick_leave += hh_sick_leave_total;
        q3_count += hh_employee_total;
      } else {
        (*itr).second->set_income_quartile(Global::Q4);
        q4_sick_leave += hh_sick_leave_total;
        q4_count += hh_employee_total;
      }

      counter++;
    }

    FRED_STATUS(0, "HOUSEHOLD INCOME QUARITLE[%d]: %.2f\n", Global::Q1,
		(q1_count == 0.0 ? 0.0 : (q1_sick_leave / q1_count)));
    FRED_STATUS(0, "HOUSEHOLD INCOME QUARITLE[%d]: %.2f\n", Global::Q2,
		(q2_count == 0.0 ? 0.0 : (q2_sick_leave / q2_count)));
    FRED_STATUS(0, "HOUSEHOLD INCOME QUARITLE[%d]: %.2f\n", Global::Q3,
		(q3_count == 0.0 ? 0.0 : (q3_sick_leave / q3_count)));
    FRED_STATUS(0, "HOUSEHOLD INCOME QUARITLE[%d]: %.2f\n", Global::Q4,
		(q4_count == 0.0 ? 0.0 : (q4_sick_leave / q4_count)));

    delete household_income_hh_mm;
  }
}

int Place_List::get_min_household_income_by_percentile(int percentile) {
  assert(this->is_load_completed());
  assert(Global::Pop.is_load_completed());
  assert(percentile > 0);
  assert(percentile <= 100);
  if(Global::Enable_hh_income_based_susc_mod) {
    typedef std::multimap<double, Household*> HouseholdMultiMapT;

    HouseholdMultiMapT* household_income_hh_mm = new HouseholdMultiMapT();
    int number_places = this->households.size();
    for(int p = 0; p < number_places; ++p) {
      Household* hh = get_household(p);
      double hh_income = hh->get_household_income();
      std::pair<double, Household*> my_insert(hh_income, hh);
      household_income_hh_mm->insert(my_insert);
    }
    int total = static_cast<int>(household_income_hh_mm->size());
    int percentile_goal = static_cast<int>((static_cast<float>(percentile) / static_cast<float>(100)) * total);
    int ret_value = 0;
    int counter = 1;
    for(HouseholdMultiMapT::iterator itr = household_income_hh_mm->begin(); itr != household_income_hh_mm->end(); ++itr) {
      double hh_sick_leave_total = 0.0;
      if(counter == percentile_goal) {
        ret_value = (*itr).second->get_household_income();
        break;
      }
      counter++;
    }
    delete household_income_hh_mm;
    return ret_value;
  }
  return -1;
}

Place* Place_List::get_place_from_label(const char* s) const {
  assert(this->place_label_map != NULL);

  if(strcmp(s, "-1") == 0) {
    return NULL;
  }
  string str(s);

  if(this->place_label_map->find(str) != this->place_label_map->end()) {
    return this->places[(*this->place_label_map)[str]];
  } else {
    FRED_VERBOSE(1, "Help!  can't find place with label = %s\n", str.c_str());
    return NULL;
  }
}

Place* Place_List::add_place(char* label, char type, char subtype, fred::geo lon, fred::geo lat, long int census_tract_fips) {

  string label_str;
  label_str.assign(label);
  if(this->place_label_map->find(label_str) != this->place_label_map->end()) {
    if (Global::Verbose > 1) {
      FRED_WARNING("duplicate place label found: %s\n", label);
    }
    return get_place_from_label(label);
  }

  Place* place = NULL;
  switch(type) {
  case 'H':
    place = new Household(label, subtype, lon, lat);
    break;

  case 'W':
    place = new Workplace(label, subtype, lon, lat);
    break;
    
  case 'O':
    place = new Office(label, subtype, lon, lat);
    break;
    
  case 'N':
    place = new Neighborhood(label, subtype, lon, lat);
    break;

  case 'S':
    place = new School(label, subtype, lon, lat);
    break;
    
  case 'C':
    place = new Classroom(label, subtype, lon, lat);
    break;
    
  case 'M':
    place = new Hospital(label, subtype, lon, lat);
    break;
  }

  int id = get_new_place_id();
  place->set_id(id);
  place->set_census_tract_fips(census_tract_fips);
  this->place_label_map->insert(std::make_pair(label_str, id));
  this->places.push_back(place);

  if(place->is_household()) {
    this->households.push_back(place);
  }

  if(place->is_neighborhood()) {
    this->neighborhoods.push_back(place);
  }

  if(place->is_school()) {
    this->schools.push_back(place);
  }

  if(place->is_workplace()) {
    this->workplaces.push_back(place);
  }

  if(place->is_hospital()) {
    this->hospitals.push_back(place);
  }
  
  FRED_VERBOSE(1, "add_place %d lab %s type %c sub %c lat %f lon %f\n",
	       place->get_id(), place->get_label(), place->get_type(), place->get_subtype(), place->get_latitude(), place->get_longitude());

  return place;
}


void Place_List::setup_group_quarters() {

  FRED_STATUS(0, "setup group quarters entered\n", "");

  // reset household indexes
  int num_households = this->households.size();
  for(int i = 0; i < num_households; ++i) {
    this->get_household(i)->set_index(i);
  }

  int p = 0;
  int units = 0;
  while(p < num_households) {
    Household* house = this->get_household(p++);
    Household* new_house;
    if(house->is_group_quarters()) {
      int gq_size = house->get_size();
      int gq_units = house->get_group_quarters_units();
      FRED_VERBOSE(1, "GQ_setup: house %d label %s subtype %c initial size %d units %d\n", p, house->get_label(),
		   house->get_subtype(), gq_size, gq_units);
      int units_filled = 1;
      if(gq_units > 1) {
	vector<Person*> housemates;
	housemates.clear();
	for(int i = 0; i < gq_size; ++i) {
	  Person* person = house->get_enrollee(i);
	  housemates.push_back(person);
	}
	int min_per_unit = gq_size / gq_units;
	int larger_units = gq_size - min_per_unit * gq_units;
        int smaller_units = gq_units - larger_units;
        FRED_VERBOSE(1, "GQ min_per_unit %d smaller = %d  larger = %d total = %d  orig = %d\n", min_per_unit,
		     smaller_units, larger_units, smaller_units*min_per_unit + larger_units*(min_per_unit+1), gq_size);
        int next_person = min_per_unit;
        for(int i = 1; i < smaller_units; ++i) {
          // assert(units_filled < gq_units);
          new_house = this->get_household(p++);
          // printf("GQ smaller new_house %s subtype %c\n", new_house->get_label(), new_house->get_subtype()); fflush(stdout);
          for(int j = 0; j < min_per_unit; ++j) {
            Person* person = housemates[next_person++];
            person->change_household(new_house);
          }
          // printf("GQ smaller new_house %s subtype %c size %d\n", new_house->get_label(), new_house->get_subtype(), new_house->get_size()); fflush(stdout);
          units_filled++;
          // printf("GQ size of smaller unit %s = %d remaining in main house %d\n",
	  // new_house->get_label(), new_house->get_size(), house->get_size());
        }
        for(int i = 0; i < larger_units; ++i) {
          new_house = this->get_household(p++);
          // printf("GQ larger new_house %s\n", new_house->get_label()); fflush(stdout);
          for(int j = 0; j < min_per_unit + 1; ++j) {
            Person* person = housemates[next_person++];
            person->change_household(new_house);
          }
          // printf("GQ larger new_house %s subtype %c size %d\n", new_house->get_label(), new_house->get_subtype(), new_house->get_size()); fflush(stdout);
          units_filled++;
          // printf("GQ size of larger unit %s = %d -- remaining in main house %d\n",
	  // new_house->get_label(), new_house->get_size(), house->get_size());
        }
      }
      units += units_filled;
    }
  }
  FRED_STATUS(0, "setup group quarters finished, units = %d\n", units);
}

// Comparison used to sort households by income below (resolve ties by place id)
static bool compare_household_incomes(Place* h1, Place* h2) {
  int inc1 = (static_cast<Household*>(h1))->get_household_income();
  int inc2 = (static_cast<Household*>(h2))->get_household_income();
  return ((inc1 == inc2) ? (h1->get_id() < h2->get_id()) : (inc1 < inc2));
}

void Place_List::setup_households() {

  FRED_STATUS(0, "setup households entered\n", "");

  int num_households = this->households.size();
  for(int p = 0; p < num_households; ++p) {
    Household* house = this->get_household(p);
    house->set_index(p);
    if(house->get_size() == 0) {
      FRED_VERBOSE(0, "Warning: house %d label %s has zero size.\n", house->get_id(), house->get_label());
      continue;
    }

    // ensure that each household has an identified householder
    Person* person_with_max_age = NULL;
    Person* head_of_household = NULL;
    int max_age = -99;
    for(int j = 0; j < house->get_size() && head_of_household == NULL; ++j) {
      Person* person = house->get_enrollee(j);
      assert(person != NULL);
      if(person->is_householder()) {
        head_of_household = person;
        continue;
      } else {
        int age = person->get_age();
        if(age > max_age) {
          max_age = age;
          person_with_max_age = person;
        }
      }
    }
    if(head_of_household == NULL) {
      assert(person_with_max_age != NULL);
      person_with_max_age->make_householder();
      head_of_household = person_with_max_age;
    }
    assert(head_of_household != NULL);

    // make sure everyone know who's the head
    for(int j = 0; j < house->get_size(); j++) {
      Person* person = house->get_enrollee(j);
      if(person != head_of_household && person->is_householder()) {
        person->set_relationship(Global::HOUSEMATE);
      }
    }
    assert(head_of_household != NULL);
    FRED_VERBOSE(1, "HOLDER: house %d label %s is_group_quarters %d householder %d age %d\n", house->get_id(),
		 house->get_label(), house->is_group_quarters()?1:0, head_of_household->get_id(), head_of_household->get_age());

    // setup household structure type
    house->set_household_structure();
    house->set_orig_household_structure();
  }

  // NOTE: the following sorts households from lowest income to highest
  std::sort(this->households.begin(), this->households.end(), compare_household_incomes);

  // reset household indexes
  for(int i = 0; i < num_households; ++i) {
    this->get_household(i)->set_index(i);
  }

  report_household_incomes();

  if(Global::Enable_Household_Shelter) {
    select_households_for_shelter();
  } else if(Global::Enable_HAZEL) {
    select_households_for_evacuation();
  }

  FRED_STATUS(0, "setup households finished\n", "");
}


void Place_List::setup_classrooms() {
  FRED_STATUS(0, "setup classrooms entered\n");
  int number_classrooms = 0;
  int number_schools = this->schools.size();
  for(int p = 0; p < number_schools; ++p) {
    School* school = get_school(p);
    school->setup_classrooms();
  }
  FRED_STATUS(0, "setup classrooms finished\n");
}


void Place_List::reassign_workers() {
  if(Global::Assign_Teachers) {
    //from: http://www.statemaster.com/graph/edu_ele_sec_pup_rat-elementary-secondary-pupil-teacher-ratio
    reassign_workers_to_schools(Place::TYPE_SCHOOL, Place_List::School_fixed_staff,
				       Place_List::School_student_teacher_ratio);
  }

  if(Global::Enable_Hospitals) {
    reassign_workers_to_places_of_type(Place::TYPE_HOSPITAL, Place_List::Hospital_fixed_staff,
				       (1.0 / Place_List::Hospital_worker_to_bed_ratio));
  }

  if(Global::Enable_Group_Quarters) {
    reassign_workers_to_group_quarters(Place::SUBTYPE_COLLEGE, Place_List::College_fixed_staff,
				       Place_List::College_resident_to_staff_ratio);
    reassign_workers_to_group_quarters(Place::SUBTYPE_PRISON, Place_List::Prison_fixed_staff,
				       Place_List::Prison_resident_to_staff_ratio);
    reassign_workers_to_group_quarters(Place::SUBTYPE_MILITARY_BASE, Place_List::Military_fixed_staff,
				       Place_List::Military_resident_to_staff_ratio);
    reassign_workers_to_group_quarters(Place::SUBTYPE_NURSING_HOME, Place_List::Nursing_home_fixed_staff,
				       Place_List::Nursing_home_resident_to_staff_ratio);
  }

  Utils::fred_print_lap_time("reassign workers");
}

void Place_List::reassign_workers_to_schools(char place_type, int fixed_staff, double staff_ratio) {
  int number_places = this->places.size();
  Utils::fred_log("reassign workers to schools entered. places = %d fixed_staff = %d staff_ratio = %f \n",
		  number_places, fixed_staff, staff_ratio);
  for(int p = 0; p < number_places; p++) {
    Place* place = this->places[p];
    if(place->get_type() == place_type) {
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      double x = Geo::get_x(lon);
      double y = Geo::get_y(lat);
      FRED_VERBOSE(0, "Reassign teachers to school %s in county %d at (%f,%f) \n",
		   place->get_label(), place->get_county_fips(), x, y);

      // ignore place if it is outside the region
      Regional_Patch* regional_patch = Global::Simulation_Region->get_patch(lat, lon);
      if(regional_patch == NULL) {
        FRED_VERBOSE(0, "school %s OUTSIDE_REGION lat %f lon %f \n",
		     place->get_label(), lat, lon);
        continue;
      }

      // target staff size
      School* s = static_cast<School*>(place);
      int n = s->get_orig_number_of_students();
      int staff = fixed_staff;
      if(staff_ratio > 0.0) {
        staff += (0.5 + (double)n / staff_ratio);
      }
      FRED_VERBOSE(1, "school %s students %d fixed_staff = %d tot_staff = %d\n",
		   place->get_label(), n, fixed_staff, staff);

      Place* nearby_workplace = regional_patch->get_nearby_workplace(place, staff);
      if(nearby_workplace != NULL) {
	// make all the workers in selected workplace teachers at the nearby school
	nearby_workplace->turn_workers_into_teachers(place);
      } else {
        FRED_VERBOSE(0, "NO NEARBY_WORKPLACE FOUND FOR SCHOOL %s in county %d at lat %f lon %f \n",
		     place->get_label(), place->get_county_fips(), lat, lon);
      }
    }
  }
}

void Place_List::reassign_workers_to_places_of_type(char place_type, int fixed_staff, double staff_ratio) {
  int number_places = this->places.size();
  Utils::fred_log("reassign workers to place of type %c entered. places = %d\n", place_type, number_places);
  for(int p = 0; p < number_places; p++) {
    Place* place = this->places[p];
    if(place->get_type() == place_type) {
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      double x = Geo::get_x(lon);
      double y = Geo::get_y(lat);
      FRED_VERBOSE(0, "Reassign workers to place %s type %c in county %d at (%f,%f) \n",
		   place->get_label(), place_type, place->get_county_fips(), x, y);

      // ignore place if it is outside the region
      Regional_Patch* regional_patch = Global::Simulation_Region->get_patch(lat, lon);
      if(regional_patch == NULL) {
        FRED_VERBOSE(0, "place OUTSIDE_REGION lat %f lon %f \n", lat, lon);
        continue;
      }

      // target staff size
      int n = place->get_size();
      if(place_type == Place::TYPE_HOSPITAL) {
        Hospital* hosp = static_cast<Hospital*>(place);
        n = hosp->get_employee_count(); // From the input file
      }
      FRED_VERBOSE(1, "Size %d\n", n);
      int staff = fixed_staff;
      if(staff_ratio > 0.0) {
        staff += (0.5 + (double)n / staff_ratio);
      }

      Place* nearby_workplace = regional_patch->get_nearby_workplace(place, staff);
      if(nearby_workplace != NULL) {
	// make all the workers in selected workplace as workers in the target place
	nearby_workplace->reassign_workers(place);
      } else {
        FRED_VERBOSE(0, "NO NEARBY_WORKPLACE FOUND for place %s in county %d at lat %f lon %f \n",
		     place->get_label(), place->get_county_fips(), lat, lon);
      }
    }
  }
}

void Place_List::reassign_workers_to_group_quarters(char subtype, int fixed_staff, double resident_to_staff_ratio) {
  int number_places = this->places.size();
  Utils::fred_log("reassign workers to group quarters subtype %c entered. places = %d\n", subtype, number_places);
  for(int p = 0; p < number_places; ++p) {
    Place* place = this->places[p];
    if(place->is_workplace() && place->get_subtype() == subtype) {
      fred::geo lat = place->get_latitude();
      fred::geo lon = place->get_longitude();
      double x = Geo::get_x(lon);
      double y = Geo::get_y(lat);
      // target staff size
      FRED_VERBOSE(1, "Size %d ", place->get_size());
      int staff = fixed_staff;
      if(resident_to_staff_ratio > 0.0) {
        staff += 0.5 + (double)place->get_size() / resident_to_staff_ratio;
      }

      FRED_VERBOSE(0, "REASSIGN WORKERS to GQ %s subtype %c target staff %d at (%f,%f) \n",
		   place->get_label(), subtype, staff, lat, lon);

      // ignore place if it is outside the region
      Regional_Patch* regional_patch = Global::Simulation_Region->get_patch(lat, lon);
      if(regional_patch == NULL) {
        FRED_VERBOSE(0, "REASSIGN WORKERS to place GQ %s subtype %c FAILED -- OUTSIDE_REGION lat %f lon %f \n",
		     place->get_label(), subtype, lat, lon);
        continue;
      }

      Place* nearby_workplace = regional_patch->get_nearby_workplace(place, staff);
      if(nearby_workplace != NULL) {
        // make all the workers in selected workplace as workers in the target place
        FRED_VERBOSE(0, "REASSIGN WORKERS: NEARBY_WORKPLACE FOUND %s for GQ %s subtype %c at lat %f lon %f \n",
		     nearby_workplace->get_label(),
		     place->get_label(), subtype, lat, lon);
        nearby_workplace->reassign_workers(place);
      }
      else {
        FRED_VERBOSE(0, "REASSIGN WORKERS: NO NEARBY_WORKPLACE FOUND for GQ %s subtype %c at lat %f lon %f \n",
		     place->get_label(), subtype, lat, lon);
      }
    }
  }
}


void Place_List::setup_offices() {
  FRED_STATUS(0, "setup offices entered\n");
  int number_workplaces = this->workplaces.size();
  for(int p = 0; p < number_workplaces; ++p) {
    Workplace* workplace = get_workplace(p);
    workplace->setup_offices();
  }
  FRED_STATUS(0, "setup offices finished\n");
}


Place* Place_List::get_random_workplace() {
  int size = static_cast<int>(this->workplaces.size());
  if(size > 0) {
    return this->workplaces[Random::draw_random_int(0, size - 1)];
  } else {
    return NULL;
  }
}

Place* Place_List::get_random_school(int grade) {
  int size = static_cast<int>(this->schools_by_grade[grade].size());
  if(size > 0) {
    return this->schools_by_grade[grade][Random::draw_random_int(0, size - 1)];
  } else {
    return NULL;
  }
}


void Place_List::assign_hospitals_to_households() {
  if(Global::Enable_Hospitals) {

    FRED_STATUS(0, "assign_hospitals_to_household entered\n");

    int number_hh = (int)this->households.size();
    for(int i = 0; i < number_hh; ++i) {
      Household* hh = static_cast<Household*>(this->households[i]);
      Hospital* hosp = static_cast<Hospital*>(this->get_hospital_assigned_to_household(hh));
      assert(hosp != NULL);
      if(hosp != NULL) {
        hh->set_household_visitation_hospital(hosp);
        string hh_label_str(hh->get_label());
        string hosp_label_str(hosp->get_label());

        this->hh_label_hosp_label_map.insert(std::pair<string, string>(hh_label_str, hosp_label_str));
      }
    }

    int number_hospitals = get_number_of_hospitals();
    int catchment_count[number_hospitals];
    double catchment_age[number_hospitals];
    double catchment_dist[number_hospitals];
    for (int i = 0; i < number_hospitals; i++) {
      catchment_count[i] = 0;
      catchment_age[i] = 0;
      catchment_dist[i] = 0;
    }

    for(int i = 0; i < number_hh; ++i) {
      Household* hh = get_household(i);
      Hospital* hosp = hh->get_household_visitation_hospital();
      assert(hosp != NULL);
      string hosp_label_str(hosp->get_label());
      int hosp_id = -1;
      if(this->hosp_label_hosp_id_map.find(hosp_label_str) != this->hosp_label_hosp_id_map.end()) {
	hosp_id = this->hosp_label_hosp_id_map.find(hosp_label_str)->second;
      }
      assert(0 <= hosp_id && hosp_id < number_hospitals);
      // printf("CATCH house %s hosp_id %d %s\n", hh->get_label(), hosp_id, hosp->get_label());
      catchment_count[hosp_id] += hh->get_size();
      catchment_dist[hosp_id] += hh->get_size()*(distance_between_places(hh,hosp));
      for (int j = 0; j < hh->get_size(); j++) {
	double age = hh->get_enrollee(j)->get_real_age();
	catchment_age[hosp_id] += age;
      }
    }

    for (int i = 0; i < number_hospitals; i++) {
      if (catchment_count[i] > 0) {
	catchment_dist[i] /= catchment_count[i];
	catchment_age[i] /= catchment_count[i];
      }
      FRED_STATUS(0,
		  "HOSPITAL CATCHMENT %d %s beds %d count %d age %f dist %f\n",
		  i, this->hospitals[i]->get_label(),
		  static_cast<Hospital*>(this->hospitals[i])->get_bed_count(0),
		  catchment_count[i],
		  catchment_age[i],
		  catchment_dist[i]);
    }

    //Write the mapping file if it did not already exist (or if it was incomplete)
    if(!Place_List::Household_hospital_map_file_exists) {

      char map_file_dir[FRED_STRING_SIZE];
      char map_file_name[FRED_STRING_SIZE];
      Params::get_param_from_string("household_hospital_map_file_directory", map_file_dir);
      Params::get_param_from_string("household_hospital_map_file", map_file_name);

      if(strcmp(map_file_name, "none") == 0) {
        this->hh_label_hosp_label_map.clear();
        return;
      }

      char filename[FRED_STRING_SIZE];
      sprintf(filename, "%s%s", map_file_dir, map_file_name);

      Utils::get_fred_file_name(filename);
      FILE* hh_label_hosp_label_map_fp = fopen(filename, "w");
      if(hh_label_hosp_label_map_fp == NULL) {
        Utils::fred_abort("Can't open %s\n", filename);
      }

      for(std::map<std::string, string>::iterator itr = this->hh_label_hosp_label_map.begin(); itr != this->hh_label_hosp_label_map.end(); ++itr) {
        fprintf(hh_label_hosp_label_map_fp, "%s,%s\n", itr->first.c_str(), itr->second.c_str());
      }

      fflush(hh_label_hosp_label_map_fp);
      fclose(hh_label_hosp_label_map_fp);
    }

    this->hh_label_hosp_label_map.clear();
    FRED_STATUS(0, "assign_hospitals_to_household finished\n");
  }
}

void Place_List::prepare_primary_care_assignment() {

  if(this->is_primary_care_assignment_initialized) {
    return;
  }

  if(Global::Enable_Hospitals && this->is_load_completed() && Global::Pop.is_load_completed()) {
    int tot_pop_size = Global::Pop.get_population_size();
    assert(Place_List::Hospital_overall_panel_size > 0);
    //Determine the distribution of population that should be assigned to each hospital location
    for(int i = 0; i < this->hospitals.size(); ++i) {
      Hospital* hosp = this->get_hospital(i);
      double proprtn_of_total_panel = 0;
      if(hosp->get_subtype() != Place::SUBTYPE_MOBILE_HEALTHCARE_CLINIC) {
        proprtn_of_total_panel = static_cast<double>(hosp->get_daily_patient_capacity(0))
	  / static_cast<double>(Place_List::Hospital_overall_panel_size);
      }
      Place_List::Hospital_ID_total_assigned_size_map.insert(std::pair<int, int>(hosp->get_id(), ceil(proprtn_of_total_panel * tot_pop_size)));
      Place_List::Hospital_ID_current_assigned_size_map.insert(std::pair<int, int>(hosp->get_id(), 0));
    }
    this->is_primary_care_assignment_initialized = true;
  }
}

Hospital* Place_List::get_random_open_hospital_matching_criteria(int sim_day, Person* per, bool check_insurance) {
  if(!Global::Enable_Hospitals) {
    return NULL;
  }

  if(check_insurance) {
    assert(Global::Enable_Health_Insurance);
  }
  assert(per != NULL);

  int overnight_cap = 0;
  Hospital* assigned_hospital = NULL;
  Household* hh = per->get_household();
  assert(hh != NULL);

  // ignore place if it is outside the region
  fred::geo lat = hh->get_latitude();
  fred::geo lon = hh->get_longitude();
  Regional_Patch* hh_patch = Global::Simulation_Region->get_patch(lat, lon);

  vector<Place*> possible_hosp = Global::Simulation_Region->get_nearby_hospitals(hh_patch->get_row(), hh_patch->get_col(), lat, lon, 5);
  int number_hospitals = static_cast<int>(possible_hosp.size());
  if(number_hospitals <= 0) {
    Utils::fred_abort("Found no nearby Hospitals in simulation that has Enabled Hospitalization", "");
  }

  int number_possible_hospitals = 0;
  //First, only try Hospitals within a certain radius (* that accept insurance)
  std::vector<double> hosp_probs;
  double probability_total = 0.0;
  for(int i = 0; i < number_hospitals; ++i) {
    Hospital* hospital = static_cast<Hospital*>(possible_hosp[i]);
    double distance = distance_between_places(hh, hospital);
    double cur_prob = 0.0;
    int increment = 0;
    overnight_cap = hospital->get_bed_count(sim_day);
    //Need to make sure place is not a healthcare clinic && there are beds available
    if(distance > 0.0 && !hospital->is_healthcare_clinic() && !hospital->is_mobile_healthcare_clinic()
       && hospital->should_be_open(sim_day)
       && (hospital->get_occupied_bed_count() < overnight_cap)) {
      if(check_insurance) {
        Insurance_assignment_index::e per_insur = per->get_health()->get_insurance_type();
        if(hospital->accepts_insurance(per_insur)) {
          //Hospital accepts the insurance so we are good
          cur_prob = static_cast<double>(overnight_cap) / distance;
          increment = 1;
        } else {
          //Not possible (Doesn't accept insurance)
          cur_prob = 0.0;
          increment = 0;
        }
      } else {
        //We don't care about insurance so good to go
        cur_prob = static_cast<double>(overnight_cap) / distance;
        increment = 1;
      }
    } else {
      //Not possible
      cur_prob = 0.0;
      increment = 0;
    }
    hosp_probs.push_back(cur_prob);
    probability_total += cur_prob;
    number_possible_hospitals += increment;
  }
  assert(static_cast<int>(hosp_probs.size()) == number_hospitals);
  FRED_VERBOSE(1,"CATCH HOSP FOR HH %s number_hospitals %d number_poss_hosp %d\n",
	       hh->get_label(), number_hospitals, number_possible_hospitals);


  if(number_possible_hospitals > 0) {
    if(probability_total > 0.0) {
      for(int i = 0; i < number_hospitals; ++i) {
        hosp_probs[i] /= probability_total;
	// printf("%f ", hosp_probs[i]);
      }
    }
    // printf("\n");

    double rand = Random::draw_random();
    double cum_prob = 0.0;
    int i = 0;
    while(i < number_hospitals) {
      cum_prob += hosp_probs[i];
      if(rand < cum_prob) {
	// printf("picked i = %d %f\n", i, hosp_probs[i]);
        return static_cast<Hospital*>(possible_hosp[i]);
      }
      ++i;
    }
    printf("HOSP CATCHMENT picked default i = %d %f\n", number_hospitals-1, hosp_probs[number_hospitals-1]);
    return static_cast<Hospital*>(possible_hosp[number_hospitals - 1]);
  } else {
    //No hospitals in the simulation match search criteria
    return NULL;
  }
}


Hospital* Place_List::get_random_open_healthcare_facility_matching_criteria(int sim_day, Person* per, bool check_insurance, bool use_search_radius_limit) {
  if(!Global::Enable_Hospitals) {
    return NULL;
  }

  if(check_insurance) {
    assert(Global::Enable_Health_Insurance);
  }
  assert(per != NULL);

  int daily_hosp_cap = 0;
  Hospital* assigned_hospital = NULL;
  int number_hospitals = this->hospitals.size();
  if(number_hospitals == 0) {
    Utils::fred_abort("No Hospitals in simulation that has Enabled Hospitalization", "");
  }
  int number_possible_hospitals = 0;
  Household* hh = per->get_household();
  assert(hh != NULL);
  //First, only try Hospitals within a certain radius (* that accept insurance)
  std::vector<double> hosp_probs;
  double probability_total = 0.0;
  for(int i = 0; i < number_hospitals; ++i) {
    Hospital* hospital = get_hospital(i);
    daily_hosp_cap = hospital->get_daily_patient_capacity(sim_day);
    double distance = distance_between_places(hh, hospital);
    double cur_prob = 0.0;
    int increment = 0;

    //Need to make sure place is open and not over capacity
    if(distance > 0.0 && hospital->should_be_open(sim_day)
       && hospital->get_current_daily_patient_count() < daily_hosp_cap) {
      if(use_search_radius_limit) {
        if(distance <= Place_List::Hospitalization_radius) {
          if(check_insurance) {
            Insurance_assignment_index::e per_insur = per->get_health()->get_insurance_type();
            if(hospital->accepts_insurance(per_insur)) {
              //Hospital accepts the insurance so we are good
              cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
              increment = 1;
            } else {
              //Not possible (Doesn't accept insurance)
              cur_prob = 0.0;
              increment = 0;
            }
          } else {
            //We don't care about insurance so good to go
            cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
            increment = 1;
          }
        } else {
          //Not possible (not within the radius)
          cur_prob = 0.0;
          increment = 0;
        }
      } else { //Don't car about search radius
        if(check_insurance) {
          Insurance_assignment_index::e per_insur = per->get_health()->get_insurance_type();
          if(hospital->accepts_insurance(per_insur)) {
            //Hospital accepts the insurance so we are good
            cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
            increment = 1;
          } else {
            //Not possible (Doesn't accept insurance)
            cur_prob = 0.0;
            increment = 0;
          }
        } else {
          //We don't care about insurance so good to go
          cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
          increment = 1;
        }
      }
    } else {
      //Not possible
      cur_prob = 0.0;
      increment = 0;
    }
    hosp_probs.push_back(cur_prob);
    probability_total += cur_prob;
    number_possible_hospitals += increment;
  } // end for loop

  assert(static_cast<int>(hosp_probs.size()) == number_hospitals);
  if(number_possible_hospitals > 0) {
    if(probability_total > 0.0) {
      for(int i = 0; i < number_hospitals; ++i) {
        hosp_probs[i] /= probability_total;
      }
    }

    double rand = Random::draw_random();
    double cum_prob = 0.0;
    int i = 0;
    while(i < number_hospitals) {
      cum_prob += hosp_probs[i];
      if(rand < cum_prob) {
        return get_hospital(i);
      }
      ++i;
    }
    return get_hospital(number_hospitals - 1);
  } else {
    //No hospitals in the simulation match search criteria
    return NULL;
  }
}


Hospital* Place_List::get_random_primary_care_facility_matching_criteria(Person* per, bool check_insurance, bool use_search_radius_limit) {
  if(!Global::Enable_Hospitals) {
    return NULL;
  }

  if(check_insurance) {
    assert(Global::Enable_Health_Insurance);
  }
  assert(per != NULL);

  //This is the initial primary care assignment
  if(!this->is_primary_care_assignment_initialized) {
    this->prepare_primary_care_assignment();
  }

  int daily_hosp_cap = 0;
  Hospital* assigned_hospital = NULL;
  int number_hospitals = this->hospitals.size();
  if(number_hospitals == 0) {
    Utils::fred_abort("No Hospitals in simulation that has Enabled Hospitalization", "");
  }
  int number_possible_hospitals = 0;
  Household* hh = per->get_household();
  assert(hh != NULL);
  //First, only try Hospitals within a certain radius (* that accept insurance)
  std::vector<double> hosp_probs;
  double probability_total = 0.0;
  for(int i = 0; i < number_hospitals; ++i) {
    Hospital* hospital = get_hospital(i);
    daily_hosp_cap = hospital->get_daily_patient_capacity(0);
    double distance = distance_between_places(hh, hospital);
    double cur_prob = 0.0;
    int increment = 0;

    //Need to make sure place is open and not over capacity
    if(distance > 0.0 && hospital->should_be_open(0)) {
      if(use_search_radius_limit) {
        if(distance <= Place_List::Hospitalization_radius) {
          if(check_insurance) {
            Insurance_assignment_index::e per_insur = per->get_health()->get_insurance_type();
            if(hospital->accepts_insurance(per_insur)) {
              //Hospital accepts the insurance so can check further
              if(Place_List::Hospital_ID_current_assigned_size_map.at(hospital->get_id())
		 < Place_List::Hospital_ID_total_assigned_size_map.at(hospital->get_id())) {
                //Hospital accepts the insurance and it hasn't been filled so we are good
                cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
                increment = 1;
              } else {
                //Not possible
                cur_prob = 0.0;
                increment = 0;
              }
            } else {
              //Not possible (Doesn't accept insurance)
              cur_prob = 0.0;
              increment = 0;
            }
          } else {
            //We don't care about insurance so can check further
            if(Place_List::Hospital_ID_current_assigned_size_map.at(hospital->get_id())
	       < Place_List::Hospital_ID_total_assigned_size_map.at(hospital->get_id())) {
              //Hospital accepts the insurance and it hasn't been filled so we are good
              cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
              increment = 1;
            } else {
              //Not possible
              cur_prob = 0.0;
              increment = 0;
            }
          }
        } else {
          //Not possible (not within the radius)
          cur_prob = 0.0;
          increment = 0;
        }
      } else { //Don't car about search radius
        if(check_insurance) {
          Insurance_assignment_index::e per_insur = per->get_health()->get_insurance_type();
          if(hospital->accepts_insurance(per_insur)) {
            //Hospital accepts the insurance so can check further
            if(Place_List::Hospital_ID_current_assigned_size_map.at(hospital->get_id())
	       < Place_List::Hospital_ID_total_assigned_size_map.at(hospital->get_id())) {
              //Hospital accepts the insurance and it hasn't been filled so we are good
              cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
              increment = 1;
            } else {
              //Not possible
              cur_prob = 0.0;
              increment = 0;
            }
          } else {
            //Not possible (Doesn't accept insurance)
            cur_prob = 0.0;
            increment = 0;
          }
        } else {
          //We don't care about insurance so can check further
          if(Place_List::Hospital_ID_current_assigned_size_map.at(hospital->get_id())
	     < Place_List::Hospital_ID_total_assigned_size_map.at(hospital->get_id())) {
            //Hospital accepts the insurance and it hasn't been filled so we are good
            cur_prob = static_cast<double>(daily_hosp_cap) / (distance * distance);
            increment = 1;
          } else {
            //Not possible
            cur_prob = 0.0;
            increment = 0;
          }
        }
      }
    } else {
      //Not possible
      cur_prob = 0.0;
      increment = 0;
    }
    hosp_probs.push_back(cur_prob);
    probability_total += cur_prob;
    number_possible_hospitals += increment;
  }  // end for loop

  assert(static_cast<int>(hosp_probs.size()) == number_hospitals);
  if(number_possible_hospitals > 0) {
    if(probability_total > 0.0) {
      for(int i = 0; i < number_hospitals; ++i) {
        hosp_probs[i] /= probability_total;
      }
    }

    double rand = Random::draw_random();
    double cum_prob = 0.0;
    int i = 0;
    while(i < number_hospitals) {
      cum_prob += hosp_probs[i];
      if(rand < cum_prob) {
        return get_hospital(i);
      }
      ++i;
    }
    return get_hospital(number_hospitals - 1);
  } else {
    //No hospitals in the simulation match search criteria
    return NULL;
  }
}


void Place_List::print_household_size_distribution(char* dir, char* date_string, int run) {
  FILE* fp;
  int count[11];
  double pct[11];
  char filename[FRED_STRING_SIZE];
  sprintf(filename, "%s/household_size_dist_%s.%02d", dir, date_string, run);
  Utils::fred_log("print_household_size_dist entered, filename = %s\n", filename);
  for(int i = 0; i < 11; ++i) {
    count[i] = 0;
  }
  int total = 0;
  int number_households = (int)households.size();
  for(int p = 0; p < number_households; ++p) {
    int n = this->households[p]->get_size();
    if(n < 11) {
      count[n]++;
    } else {
      count[10]++;
    }
    total++;
  }
  fp = fopen(filename, "w");
  for(int i = 0; i < 11; i++) {
    pct[i] = (100.0 * count[i]) / number_households;
    fprintf(fp, "size %d count %d pct %f\n", i * 5, count[i], pct[i]);
  }
  fclose(fp);
}

void Place_List::delete_place_label_map() {
  if(this->place_label_map) {
    delete this->place_label_map;
    this->place_label_map = NULL;
  }
}

void Place_List::get_initial_visualization_data_from_households() {
  int num_households = this->households.size();
  for(int i = 0; i < num_households; ++i) {
    Household* h = this->get_household(i);
    Global::Visualization->initialize_household_data(h->get_latitude(), h->get_longitude(), h->get_size());
    // printf("%f %f %3d %s\n", h->get_latitude(), h->get_longitude(), h->get_size(), h->get_label());
  }
}

void Place_List::get_visualization_data_from_households(int day, int condition_id, int output_code) {
  int num_households = this->households.size();
  for(int i = 0; i < num_households; ++i) {
    Household* h = this->get_household(i);
    int count = h->get_visualization_counter(day, condition_id, output_code);
    int popsize = h->get_size();
    // update appropriate visualization patch
    Global::Visualization->update_data(h->get_latitude(), h->get_longitude(), count, popsize);
  }
}

void Place_List::get_census_tract_data_from_households(int day, int condition_id, int output_code) {
  int num_households = this->households.size();
  for(int i = 0; i < num_households; ++i) {
    Household* h = this->get_household(i);
    int count = h->get_visualization_counter(day, condition_id, output_code);
    int popsize = h->get_size();
    long int census_tract_fips = h->get_census_tract_fips();
    Global::Visualization->update_data(census_tract_fips, count, popsize);
  }
}

void Place_List::report_household_incomes() {

  // initialize household income stats
  this->min_household_income = 0;
  this->max_household_income = 0;
  this->median_household_income = 0;
  this->first_quartile_household_income = 0;
  this->third_quartile_household_income = 0;

  int num_households = this->households.size();
  if(num_households > 0) {
    this->min_household_income = this->get_household(0)->get_household_income();
    this->max_household_income = this->get_household(num_households - 1)->get_household_income();
    this->first_quartile_household_income = this->get_household(num_households / 4)->get_household_income();
    this->median_household_income = this->get_household(num_households / 2)->get_household_income();
    this->third_quartile_household_income = this->get_household((3 * num_households) / 4)->get_household_income();
  }

  // print household incomes to LOG file
  if(Global::Verbose > 1) {
    for(int i = 0; i < num_households; ++i) {
      Household* h = this->get_household(i);
      int h_county = h->get_county_fips();
      FRED_VERBOSE(0, "INCOME: %s %c %f %f %d %d\n", h->get_label(), h->get_type(), h->get_latitude(),
		   h->get_longitude(), h->get_household_income(), h_county);
    }
  }
  FRED_VERBOSE(0, "INCOME_STATS: households: %d  min %d  first_quartile %d  median %d  third_quartile %d  max %d\n",
	       num_households, min_household_income, first_quartile_household_income, median_household_income,
	       third_quartile_household_income, max_household_income);

}

void Place_List::select_households_for_shelter() {
  FRED_VERBOSE(0, "select_households_for_shelter entered.\n");
  FRED_VERBOSE(0, "pct_households_sheltering = %f\n", Place_List::Pct_households_sheltering);
  FRED_VERBOSE(0, "num_households = %d\n", this->households.size());
  int num_sheltering = 0.5 + Place_List::Pct_households_sheltering * this->households.size();
  FRED_VERBOSE(0, "num_sheltering = %d\n", num_sheltering);
  FRED_VERBOSE(0, "high_income = %d\n", Place_List::High_income_households_sheltering?1:0);

  int num_households = this->households.size();

  if(Place_List::High_income_households_sheltering) {
    // this assumes that household have been sorted in increasing income
    // in setup_households()
    for(int i = 0; i < num_sheltering; ++i) {
      int j = num_households - 1 - i;
      Household* h = get_household(j);
      shelter_household(h);
    }
  } else {
    // select households randomly
    vector<Household*> tmp;
    tmp.clear();
    for(int i = 0; i < this->households.size(); ++i) {
      tmp.push_back(this->get_household(i));
    }
    // randomly shuffle selected households
    FYShuffle<Household*>(tmp);
    for(int i = 0; i < num_sheltering; ++i) {
      this->shelter_household(tmp[i]);
    }
  }
  FRED_VERBOSE(0, "select_households_for_shelter finished.\n");
}

void Place_List::shelter_household(Household* h) {
  h->set_shelter(true);

  // set shelter delay
  int shelter_start_day = 0.4999999
    + Random::draw_normal(Place_List::Shelter_delay_mean, Place_List::Shelter_delay_std);
  if(Place_List::Early_shelter_rate > 0.0) {
    double r = Random::draw_random();
    while(shelter_start_day > 0 && r < Place_List::Early_shelter_rate) {
      shelter_start_day--;
      r = Random::draw_random();
    }
  }
  if(shelter_start_day < 0) {
    shelter_start_day = 0;
  }
  h->set_shelter_start_day(shelter_start_day);

  // set shelter duration
  int shelter_duration = 0.4999999
    + Random::draw_normal(Place_List::Shelter_duration_mean, Place_List::Shelter_duration_std);
  if(shelter_duration < 1) {
    shelter_duration = 1;
  }

  if(Place_List::Shelter_decay_rate > 0.0) {
    double r = Random::draw_random();
    if(r < 0.5) {
      shelter_duration = 1;
      r = Random::draw_random();
      while(shelter_duration < Place_List::Shelter_duration_mean && Place_List::Shelter_decay_rate < r) {
        shelter_duration++;
        r = Random::draw_random();
      }
    }
  }
  h->set_shelter_end_day(shelter_start_day + shelter_duration);

  FRED_VERBOSE(1, "ISOLATE household %s size %d income %d ", h->get_label(), h->get_size(), h->get_household_income());
  FRED_VERBOSE(1, "start_day %d end_day %d duration %d ", h->get_shelter_start_day(), h->get_shelter_end_day(),
	       h->get_shelter_end_day()-h->get_shelter_start_day());
}

void Place_List::select_households_for_evacuation() {
  if(!Global::Enable_HAZEL) {
    return;
  }

  FRED_VERBOSE(0, "HAZEL: select_households_for_evacuation entered.\n");
  int num_households = this->households.size();
  int evac_start_sim_day = Place_List::HAZEL_disaster_start_sim_day + Place_List::HAZEL_disaster_evac_start_offset;
  int evac_end_sim_day = Place_List::HAZEL_disaster_end_sim_day + Place_List::HAZEL_disaster_evac_end_offset;
  int return_start_sim_day = Place_List::HAZEL_disaster_end_sim_day + Place_List::HAZEL_disaster_return_start_offset;
  int return_end_sim_day = Place_List::HAZEL_disaster_end_sim_day + Place_List::HAZEL_disaster_return_end_offset;
  int count_hh_evacuating = 0;

  FRED_VERBOSE(0, "HAZEL: HAZEL_disaster_start_sim_day = %d\n", Place_List::HAZEL_disaster_start_sim_day);
  FRED_VERBOSE(0, "HAZEL: HAZEL_disaster_evac_start_offset = %d\n", Place_List::HAZEL_disaster_evac_start_offset);
  FRED_VERBOSE(0, "HAZEL: HAZEL_disaster_end_sim_day = %d\n", Place_List::HAZEL_disaster_end_sim_day);
  FRED_VERBOSE(0, "HAZEL: HAZEL_disaster_evac_end_offset = %d\n", Place_List::HAZEL_disaster_evac_end_offset);
  FRED_VERBOSE(0, "HAZEL: HAZEL_disaster_return_start_offset = %d\n", Place_List::HAZEL_disaster_return_start_offset);
  FRED_VERBOSE(0, "HAZEL: HAZEL_disaster_return_end_offset = %d\n", Place_List::HAZEL_disaster_return_end_offset);
  FRED_VERBOSE(0, "HAZEL: evac_start_sim_day = %d\n", evac_start_sim_day);
  FRED_VERBOSE(0, "HAZEL: evac_end_sim_day = %d\n", evac_end_sim_day);
  FRED_VERBOSE(0, "HAZEL: return_start_sim_day = %d\n", return_start_sim_day);
  FRED_VERBOSE(0, "HAZEL: return_end_sim_day = %d\n", return_end_sim_day);
  if(evac_start_sim_day < 0 || evac_end_sim_day < evac_start_sim_day) {
    return;
  }

  for(int i = 0; i < num_households; ++i) {
    Household* tmp_hh = this->get_household(i);
    bool evac_date_set = false;
    bool return_date_set = false;
    for(int j = evac_start_sim_day; j <= evac_end_sim_day; ++j) {
      if(Random::draw_random() < Place_List::HAZEL_disaster_evac_prob_per_day) {
        tmp_hh->set_shelter_start_day(j);
        evac_date_set = true;
        count_hh_evacuating++;
        for(int k = return_start_sim_day; k <= return_end_sim_day; ++k) {
          if(Random::draw_random() < Place_List::HAZEL_disaster_evac_prob_per_day || k == return_end_sim_day) {
            if(k > j) { //Can't return before you leave
              tmp_hh->set_shelter_end_day(k);
              return_date_set = true;
            }
          }
          if(return_date_set) {
            break;
          }
        }
        if(evac_date_set) {
          assert(return_date_set);
          break;
        }
      }
    }
  }

  FRED_VERBOSE(0, "HAZEL: num_households = %d\n", num_households);
  FRED_VERBOSE(0, "HAZEL: num_evacuating = %d\n", count_hh_evacuating);
  FRED_VERBOSE(0, "HAZEL: pct_households_evacuating = %f\n",
	       static_cast<float>(count_hh_evacuating) / static_cast<float>(num_households));
  FRED_VERBOSE(0, "HAZEL: select_households_for_evacuation finished.\n");
}

void Place_List::report_shelter_stats(int day) {
  int sheltering_households = 0;
  int sheltering_pop = 0;
  int sheltering_total_pop = 0;
  int sheltering_new_infections = 0;
  int sheltering_total_infections = 0;
  int non_sheltering_total_infections = 0;
  int non_sheltering_pop = 0;
  int non_sheltering_new_infections = 0;
  int num_households = this->households.size();
  double sheltering_ar = 0.0;
  double non_sheltering_ar = 0.0;
  for(int i = 0; i < num_households; ++i) {
    Household* h = this->get_household(i);
    if(h->is_sheltering()) {
      sheltering_new_infections += h->get_new_infections(day,0);
      sheltering_total_infections += h->get_total_infections(0);
      sheltering_total_pop += h->get_size();
    } else {
      non_sheltering_pop += h->get_size();
      non_sheltering_new_infections += h->get_new_infections(day,0);
      non_sheltering_total_infections += h->get_total_infections(0);
    }
    if(h->is_sheltering_today(day)) {
      sheltering_households++;
      sheltering_pop += h->get_size();
    }
  }
  if(sheltering_total_pop > 0) {
    sheltering_ar = 100.0 * (double)sheltering_total_infections / static_cast<double>(sheltering_total_pop);
  }
  if(non_sheltering_pop > 0) {
    non_sheltering_ar = 100.0 * (double)non_sheltering_total_infections / static_cast<double>(non_sheltering_pop);
  }
  Global::Daily_Tracker->set_index_key_pair(day, "H_sheltering", sheltering_households);
  Global::Daily_Tracker->set_index_key_pair(day, "N_sheltering", sheltering_pop);
  Global::Daily_Tracker->set_index_key_pair(day, "C_sheltering", sheltering_new_infections);
  Global::Daily_Tracker->set_index_key_pair(day, "AR_sheltering", sheltering_ar);
  Global::Daily_Tracker->set_index_key_pair(day, "N_noniso", non_sheltering_pop);
  Global::Daily_Tracker->set_index_key_pair(day, "C_noniso", non_sheltering_new_infections);
  Global::Daily_Tracker->set_index_key_pair(day, "AR_noniso", non_sheltering_ar);
}

void Place_List::end_of_run() {
  if(Global::Verbose > 1) {
    int number_places = this->places.size();
    for(int p = 0; p < number_places; ++p) {
      Place* place = this->places[p];
      fprintf(Global::Statusfp,
	      "PLACE REPORT: id %d type %c size %d inf %d attack_rate %5.2f first_day %d last_day %d\n",
	      place->get_id(), place->get_type(), place->get_size(),
	      place->get_total_infections(0),
	      100.0 * place->get_attack_rate(0),
	      place->get_first_day_infectious(),
	      place->get_last_day_infectious());
    }
  }
  if(Global::Enable_Household_Shelter) {
    int households_sheltering = 0;
    int households_not_sheltering = 0;
    int pop_sheltering = 0;
    int pop_not_sheltering = 0;
    int infections_sheltering = 0;
    int infections_not_sheltering = 0;
    double ar_sheltering = 0.0;
    double ar_not_sheltering = 0.0;
    int num_households = this->households.size();
    for(int i = 0; i < num_households; ++i) {
      Household* h = this->get_household(i);
      if(h->is_sheltering()) {
        pop_sheltering += h->get_size();
        infections_sheltering += h->get_total_infections(0);
        households_sheltering++;
      } else {
        pop_not_sheltering += h->get_size();
        infections_not_sheltering += h->get_total_infections(0);
        households_not_sheltering++;
      }
    }

    if(pop_sheltering > 0) {
      ar_sheltering = (double)infections_sheltering / (double)pop_sheltering;
    }

    if(pop_not_sheltering > 0) {
      ar_not_sheltering = (double)infections_not_sheltering / (double)pop_not_sheltering;
    }

    fprintf(Global::Statusfp,
	    "ISOLATION REPORT: households_sheltering %d pop_sheltering %d infections_sheltering %d ar_sheltering %f ",
	    households_sheltering, pop_sheltering, infections_sheltering, ar_sheltering);
    fprintf(Global::Statusfp,
	    "households_not_sheltering %d pop_not_sheltering %d infections_not_sheltering %d ar_not_sheltering %f\n",
	    households_not_sheltering, pop_not_sheltering, infections_not_sheltering, ar_not_sheltering);
    fflush(Global::Statusfp);
  }
}

int Place_List::get_housing_data(int* target_size, int* current_size) {
  int num_households = this->households.size();
  for(int i = 0; i < num_households; ++i) {
    Household* h = this->get_household(i);
    current_size[i] = h->get_size();
    target_size[i] = h->get_orig_size();
  }
  return num_households;
}

void Place_List::swap_houses(int house_index1, int house_index2) {

  Household* h1 = this->get_household(house_index1);
  Household* h2 = this->get_household(house_index2);
  if(h1 == NULL || h2 == NULL)
    return;

  FRED_VERBOSE(1, "HOUSING: swapping house %s with %d beds and %d occupants with %s with %d beds and %d occupants\n",
	       h1->get_label(), h1->get_orig_size(), h1->get_size(), h2->get_label(), h2->get_orig_size(), h2->get_size());

  // get pointers to residents of house h1
  vector<Person*> temp1;
  temp1.clear();
  vector<Person*> housemates1 = h1->get_inhabitants();
  for(std::vector<Person*>::iterator itr = housemates1.begin(); itr != housemates1.end(); ++itr) {
    temp1.push_back(*itr);
  }

  // get pointers to residents of house h2
  vector<Person*> temp2;
  temp2.clear();
  vector<Person *> housemates2 = h2->get_inhabitants();
  for(std::vector<Person*>::iterator itr = housemates2.begin(); itr != housemates2.end(); ++itr) {
    temp2.push_back(*itr);
  }

  // move first group into house h2
  for(std::vector<Person*>::iterator itr = temp1.begin(); itr != temp1.end(); ++itr) {
    (*itr)->change_household(h2);
  }

  // move second group into house h1
  for(std::vector<Person*>::iterator itr = temp2.begin(); itr != temp2.end(); ++itr) {
    (*itr)->change_household(h1);
  }

  FRED_VERBOSE(1, "HOUSING: swapped house %s with %d beds and %d occupants with %s with %d beds and %d occupants\n",
	       h1->get_label(), h1->get_orig_size(), h1->get_size(), h2->get_label(), h2->get_orig_size(), h2->get_size());
}

void Place_List::swap_houses(Household* h1, Household* h2) {

  if(h1 == NULL || h2 == NULL)
    return;

  FRED_VERBOSE(0, "HOUSING: swapping house %s with %d beds and %d occupants with %s with %d beds and %d occupants\n",
	       h1->get_label(), h1->get_orig_size(), h1->get_size(), h2->get_label(), h2->get_orig_size(), h2->get_size());

  // get pointers to residents of house h1
  vector<Person*> temp1;
  temp1.clear();
  vector<Person*> housemates1 = h1->get_inhabitants();
  for(std::vector<Person*>::iterator itr = housemates1.begin(); itr != housemates1.end(); ++itr) {
    temp1.push_back(*itr);
  }

  // get pointers to residents of house h2
  vector<Person*> temp2;
  temp2.clear();
  vector<Person *> housemates2 = h2->get_inhabitants();
  for(std::vector<Person*>::iterator itr = housemates2.begin(); itr != housemates2.end(); ++itr) {
    temp2.push_back(*itr);
  }

  // move first group into house h2
  for(std::vector<Person*>::iterator itr = temp1.begin(); itr != temp1.end(); ++itr) {
    (*itr)->change_household(h2);
  }

  // move second group into house h1
  for(std::vector<Person*>::iterator itr = temp2.begin(); itr != temp2.end(); ++itr) {
    (*itr)->change_household(h1);
  }

  FRED_VERBOSE(1, "HOUSING: swapped house %s with %d beds and %d occupants with %s with %d beds and %d occupants\n",
	       h1->get_label(), h1->get_orig_size(), h1->get_size(), h2->get_label(), h2->get_orig_size(), h2->get_size());
}

void Place_List::combine_households(int house_index1, int house_index2) {

  Household* h1 = this->get_household(house_index1);
  Household* h2 = this->get_household(house_index2);
  if(h1 == NULL || h2 == NULL)
    return;

  FRED_VERBOSE(1, "HOUSING: combining house %s with %d beds and %d occupants with %s with %d beds and %d occupants\n",
	       h1->get_label(), h1->get_orig_size(), h1->get_size(), h2->get_label(), h2->get_orig_size(), h2->get_size());

  // get pointers to residents of house h2
  vector<Person*> temp2;
  temp2.clear();
  vector<Person*> housemates2 = h2->get_inhabitants();
  for(std::vector<Person*>::iterator itr = housemates2.begin(); itr != housemates2.end(); ++itr) {
    temp2.push_back(*itr);
  }

  // move into house h1
  for(std::vector<Person*>::iterator itr = temp2.begin(); itr != temp2.end(); ++itr) {
    (*itr)->change_household(h1);
  }

  printf("HOUSING: combined house %s with %d beds and %d occupants with %s with %d beds and %d occupants\n",
	 h1->get_label(), h1->get_orig_size(), h1->get_size(), h2->get_label(), h2->get_orig_size(), h2->get_size());

}

Hospital* Place_List::get_hospital_assigned_to_household(Household* hh) {
  assert(this->is_load_completed());
  if(this->hh_label_hosp_label_map.find(string(hh->get_label())) != this->hh_label_hosp_label_map.end()) {
    string hosp_label = this->hh_label_hosp_label_map.find(string(hh->get_label()))->second;
    if(this->hosp_label_hosp_id_map.find(hosp_label) != this->hosp_label_hosp_id_map.end()) {
      int hosp_id = this->hosp_label_hosp_id_map.find(hosp_label)->second;
      return static_cast<Hospital*>(this->get_hospital(hosp_id));
    } else {
      return NULL;
    }
  } else {
    if(Place_List::Household_hospital_map_file_exists) {
      //List is incomplete so set this so we can print out a new file
      Place_List::Household_hospital_map_file_exists = false;
    }

    Hospital* hosp = NULL;
    if(hh->get_size() > 0) {
      Person* per = hh->get_enrollee(0);
      assert(per != NULL);
      if(Global::Enable_Health_Insurance) {
        hosp = this->get_random_open_hospital_matching_criteria(0, per, true);
      } else {
        hosp = this->get_random_open_hospital_matching_criteria(0, per, false);
      }

      //If it still came back with nothing, ignore health insurance
      if(hosp == NULL) {
        hosp = this->get_random_open_hospital_matching_criteria(0, per, false);
      }
    }
    assert(hosp != NULL);
    return hosp;
  }
}

void Place_List::update_population_dynamics(int day) {

  if(!Global::Enable_Population_Dynamics) {
    return;
  }

  int number_counties = this->counties.size();
  for(int i = 0; i < number_counties; ++i) {
    this->counties[i]->update(day);
  }

}

int Place_List::get_HAZEL_disaster_start_sim_day() {
  return Place_List::HAZEL_disaster_start_sim_day;
}

int Place_List::get_HAZEL_disaster_end_sim_day() {
  return Place_List::HAZEL_disaster_end_sim_day;
}

void Place_List::setup_HAZEL_mobile_vans() {
  int num_hospitals = static_cast<int>(this->hospitals.size());
  vector<Hospital*> temp_hosp_vec;
  int count = 0;
  for(int i = 0; i < num_hospitals; ++i) {
    Hospital* tmp_hosp = this->get_hospital(i);
    if(tmp_hosp->is_mobile_healthcare_clinic()) {
      temp_hosp_vec.push_back(tmp_hosp);
      count++;
    }
  }

  //If the max number of Mobile vans allowed is >= the total mobile vans in the system, then activate all of them
  if(Place_List::HAZEL_mobile_van_max >= static_cast<int>(temp_hosp_vec.size())) {
    for(int i = 0; i < static_cast<int>(temp_hosp_vec.size()); ++i) {
      //The Mobile Healthcare Clinics close after days
      temp_hosp_vec.at(i)->set_close_date(Place_List::HAZEL_disaster_end_sim_day + Hospital::get_HAZEL_mobile_van_open_delay()
					  + Hospital::get_HAZEL_mobile_van_closure_day());
      temp_hosp_vec.at(i)->set_open_date(Global::Days);
      temp_hosp_vec.at(i)->have_HAZEL_closure_dates_been_set(true);
    }
  } else {
    //shuffle the vector
    std::random_shuffle(temp_hosp_vec.begin(), temp_hosp_vec.end());
    for(int i = 0; i < Place_List::HAZEL_mobile_van_max; ++i) {
      //The Mobile Healthcare Clinics close after days
      temp_hosp_vec.at(i)->set_close_date(Place_List::HAZEL_disaster_end_sim_day + Hospital::get_HAZEL_mobile_van_open_delay()
					  + Hospital::get_HAZEL_mobile_van_closure_day());
      temp_hosp_vec.at(i)->set_open_date(Global::Days);
      temp_hosp_vec.at(i)->have_HAZEL_closure_dates_been_set(true);
    }
    for(int i = Place_List::HAZEL_mobile_van_max; i < static_cast<int>(temp_hosp_vec.size()); ++i) {
      //These Mobile Healthcare Clinic will never open
      temp_hosp_vec.at(i)->set_close_date(0);
      temp_hosp_vec.at(i)->set_open_date(Global::Days);
      temp_hosp_vec.at(i)->have_HAZEL_closure_dates_been_set(true);
    }
  }
}

void Place_List::print_stats(int day) {

  if(Global::Enable_HAZEL) {
    int num_open_hosp = 0;
    int open_hosp_cap = 0;
    int tot_hosp_cap = 0;
    int num_hospitals = static_cast<int>(this->hospitals.size());
    for(int i = 0; i < num_hospitals; ++i) {
      Hospital* tmp_hosp = this->get_hospital(i);
      int hosp_cap = tmp_hosp->get_daily_patient_capacity(day);
      if(tmp_hosp->should_be_open(day)) {
        num_open_hosp++;
        open_hosp_cap += hosp_cap;
        tot_hosp_cap += hosp_cap;
      } else {
        tot_hosp_cap += hosp_cap;
      }
    }

    int num_households = this->households.size();
    int tot_res_stayed = 0;
    int tot_res_evac = 0;

    for(int i = 0; i < num_households; ++i) {
      Household* hh = this->get_household(i);
      if(hh->is_sheltering_today(day)) {
        tot_res_evac += hh->get_size();
      } else {
        tot_res_stayed += hh->get_size();
      }
    }

    FRED_VERBOSE(1, "Place_List print stats for day %d\n", day);
    Global::Daily_Tracker->set_index_key_pair(day, "Tot_hosp_cap", tot_hosp_cap);
    Global::Daily_Tracker->set_index_key_pair(day, "Open_hosp_cap", open_hosp_cap);
    Global::Daily_Tracker->set_index_key_pair(day, "Open_hosp", num_open_hosp);
    Global::Daily_Tracker->set_index_key_pair(day, "Closed_hosp", num_hospitals - num_open_hosp);
    Global::Daily_Tracker->set_index_key_pair(day, "Tot_res_stayed", tot_res_stayed);
    Global::Daily_Tracker->set_index_key_pair(day, "Tot_res_evac", tot_res_evac);
  }
}

///////////////////// County Methods 

int Place_List::get_fips_of_county_with_index(int index) {
  if(index < 0) {
    return 99999;
  }
  assert(index < this->counties.size());
  return this->counties[index]->get_fips();
}

int Place_List::get_population_of_county_with_index(int index) {
  if(index < 0) {
    return 0;
  }
  assert(index < this->counties.size());
  return this->counties[index]->get_current_popsize();
}

int Place_List::get_population_of_county_with_index(int index, int age) {
  if(index < 0) {
    return 0;
  }
  assert(index < this->counties.size());
  int retval = this->counties[index]->get_current_popsize(age);
  return (retval < 0 ? 0 : retval);
}

int Place_List::get_population_of_county_with_index(int index, int age, char sex) {
  if(index < 0) {
    return 0;
  }
  assert(index < this->counties.size());
  int retval = this->counties[index]->get_current_popsize(age, sex);
  return (retval < 0 ? 0 : retval);
}

int Place_List::get_population_of_county_with_index(int index, int age_min, int age_max, char sex) {
  if(index < 0) {
    return 0;
  }
  assert(index < this->counties.size());
  int retval = this->counties[index]->get_current_popsize(age_min, age_max, sex);
  return (retval < 0 ? 0 : retval);
}

void Place_List::increment_population_of_county_with_index(int index, Person* person) {
  if(index < 0) {
    return;
  }
  assert(index < this->counties.size());
  int fips = this->counties[index]->get_fips();
  bool test = this->counties[index]->increment_popsize(person);
  assert(test);
  return;
}

void Place_List::decrement_population_of_county_with_index(int index, Person* person) {
  if(index < 0) {
    return;
  }
  assert(index < this->counties.size());
  bool test = this->counties[index]->decrement_popsize(person);
  assert(test);
  return;
}

void Place_List::report_county_populations() {
  for(int index = 0; index < this->counties.size(); ++index) {
    this->counties[index]->report_county_population();
  }
}

void Place_List::update_geo_boundaries(fred::geo lat, fred::geo lon) {
  // update max and min geo coords
  if(lat != 0.0) {
    if(lat < this->min_lat) {
      this->min_lat = lat;
    }
    if(this->max_lat < lat) {
      this->max_lat = lat;
    }
  }
  if(lon != 0.0) {
    if(lon < this->min_lon) {
      this->min_lon = lon;
    }
    if(this->max_lon < lon) {
      this->max_lon = lon;
    }
  }
  return;
}
